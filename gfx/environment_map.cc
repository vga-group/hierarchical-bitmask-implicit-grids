#include "environment_map.hh"
#include "vulkan_helpers.hh"
#include "alias_table_importance.comp.h"
#include "equirectangular_to_cubemap_convert.comp.h"
#include <limits>

namespace rb::gfx
{
struct rendered {};

environment_map::environment_map(
    const texture* cubemap,
    parallax_type parallax
):  cubemap(cubemap), guard_radius(0.1), parallax(parallax),
    clip_range(0, std::numeric_limits<float>::infinity()),
    current_render_samples(0), total_render_samples(0),
    current_filter_samples(0), total_filter_samples(0)
{
    RB_CHECK(
        cubemap && cubemap->get_view_type() != VK_IMAGE_VIEW_TYPE_CUBE,
        "set_cubemap() called but given texture is not a cubemap!"
    );
}

environment_map::~environment_map()
{
}

aabb environment_map::get_aabb(const transformable& self) const
{
    return aabb_from_obb(aabb{vec3(-1-guard_radius), vec3(1+guard_radius)}, self.get_global_transform());
}

bool environment_map::point_inside(const transformable& self, vec3 p) const
{
    mat4 gt = self.get_global_transform();
    mat4 m = glm::affineInverse(gt);
    vec3 mp = abs(vec3(m * vec4(p, 1))) - guard_radius;
    return all(lessThan(mp, vec3(1)));
}

void environment_map::refresh(unsigned render_samples, unsigned filter_samples)
{
    current_render_samples = 0;
    total_render_samples = render_samples;
    current_filter_samples = 0;
    total_filter_samples = filter_samples;
}

void environment_map::refresh_filter(unsigned filter_samples)
{
    current_filter_samples = 0;
    total_filter_samples = filter_samples;
}

entity find_sky_envmap(scene& s)
{
    entity env_id = INVALID_ENTITY;
    float largest = 0;
    s.foreach([&](entity id, rendered&, transformable* t, environment_map& em){
        vec3 radius = t->get_global_scaling();
        float volume = radius.x*radius.y*radius.z;
        if(volume > largest)
        {
            env_id = id;
            largest = volume;
        }
    });
    return env_id;
}

struct gpu_alias_table_entry
{
    uint32_t alias_id;
    uint32_t probability;
    float pdf;
    float alias_pdf;
};

environment_map_alias_table_generator::environment_map_alias_table_generator(device& dev)
:   dev(&dev),
    envmap_sampler(dev, VK_FILTER_NEAREST, VK_FILTER_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, 0, 0),
    importance_pipeline(dev),
    importance_set(dev)
{
    shader_data importance_shader = alias_table_importance_comp_shader_binary;
    importance_set.add(importance_shader);
    importance_pipeline.init(importance_shader, 0, {importance_set.get_layout()});
}

void environment_map_alias_table_generator::generate(
    environment_map_alias_table& emat,
    const texture& cubemap
){
    //RB_LOG("Start generating alias table");

    uvec2 size = uvec2(cubemap.get_dim());
    unsigned pixel_count = 6 * size.x * size.y;
    size_t bytes = sizeof(gpu_alias_table_entry) * pixel_count;
    emat.alias_table = create_gpu_buffer(
        cubemap.get_device(),
        bytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof(gpu_alias_table_entry)
    );
    vkres<VkBuffer> readback_buffer = create_readback_buffer(*dev, bytes);

    vkres<VkCommandBuffer> cmd = begin_command_buffer(*dev);

    importance_pipeline.bind(cmd);
    importance_set.set_texture("environment", cubemap, envmap_sampler);
    importance_set.set_buffer("alias_table", (VkBuffer)emat.alias_table);
    importance_pipeline.push_descriptors(cmd, importance_set);

    importance_pipeline.dispatch(cmd, uvec3((size+15u)/16u, 6));

    buffer_barrier(cmd, emat.alias_table);
    VkBufferCopy region = {0, 0, pixel_count * sizeof(float)};
    vkCmdCopyBuffer(cmd, emat.alias_table, readback_buffer, 1, &region);

    end_command_buffer(*dev, cmd).wait(*dev);

    const VmaAllocator& allocator = dev->allocator;
    float* importance = nullptr;
    vmaMapMemory(allocator, readback_buffer.get_allocation(), (void**)&importance);

    double sum_importance = 0;
    for(unsigned i = 0; i < pixel_count; ++i)
        sum_importance += importance[i];

    double average = sum_importance / double(pixel_count);
    emat.average_luminance = sum_importance;
    float inv_average = 1.0 / average;
    for(unsigned i = 0; i < pixel_count; ++i)
        importance[i] *= inv_average;

    // Average of 'probability' is now 1.
    // Sweeping alias table build idea from: https://arxiv.org/pdf/1903.00227.pdf
    std::vector<gpu_alias_table_entry> at_data(pixel_count);
    for(unsigned i = 0; i < pixel_count; ++i)
        at_data[i] = {i, 0xFFFFFFFF, 1.0f, 1.0f};

    // i tracks light items, j tracks heavy items.
    unsigned i = 0, j = 0;
    while(i < pixel_count && importance[i] > 1.0f) ++i;
    while(j < pixel_count && importance[j] <= 1.0f) ++j;

    float weight = j < pixel_count ? importance[j] : 0.0f;
    while(j < pixel_count)
    {
        if(weight > 1.0f)
        {
            if(i > pixel_count) break;
            at_data[i].probability = ldexp(importance[i], 32);
            at_data[i].alias_id = j;
            weight = (weight + importance[i]) - 1.0f;
            ++i;
            while(i < pixel_count && importance[i]> 1.0f) ++i;
        }
        else
        {
            at_data[j].probability = ldexp(weight, 32);
            unsigned old_j = j;
            ++j;
            while(j < pixel_count && importance[j] <= 1.0f) ++j;
            if(j < pixel_count)
            {
                at_data[old_j].alias_id = j;
                weight = (weight + importance[j]) - 1.0f;
            }
        }
    }

    // Write inverse pdfs.
    for(unsigned i = 0; i < pixel_count; ++i)
    {
        at_data[i].pdf = importance[i];
        at_data[i].alias_pdf = importance[at_data[i].alias_id];
    }

    memcpy(importance, at_data.data(), bytes);

    vmaUnmapMemory(allocator, readback_buffer.get_allocation());

    cmd = begin_command_buffer(*dev);
    region.size = bytes;
    vkCmdCopyBuffer(cmd, readback_buffer, emat.alias_table, 1, &region);
    end_command_buffer(*dev, cmd).wait(*dev);

    //RB_LOG("Finish generating alias table");
}

equirectangular_to_cubemap_converter::equirectangular_to_cubemap_converter(device& dev)
:   dev(&dev), envmap_sampler(dev), pipeline(dev), set(dev)
{
    shader_data convert_shader = equirectangular_to_cubemap_convert_comp_shader_binary;
    set.add(convert_shader);
    pipeline.init(convert_shader, 0, {set.get_layout()});
}

texture equirectangular_to_cubemap_converter::convert(const texture& equirectangular, int target_resolution)
{
    if(target_resolution < 0)
        target_resolution = equirectangular.get_size().x/4;

    texture::options opt;
    opt.dim = uvec3(target_resolution, target_resolution, 1);
    opt.format = equirectangular.get_format();
    opt.type = VK_IMAGE_VIEW_TYPE_CUBE;
    opt.mipmapped = false;
    opt.opaque = true;
    texture ret(*dev, opt);

    vkres<VkCommandBuffer> cmd = begin_command_buffer(*dev);

    pipeline.bind(cmd);
    set.set_texture("src", equirectangular, envmap_sampler);
    set.set_image("dst", ret);
    pipeline.push_descriptors(cmd, set);

    pipeline.dispatch(cmd, uvec3(
        (target_resolution+15u)/16u,
        (target_resolution+15u)/16u,
        6
    ));

    image_barrier(cmd, ret.get_image(), ret.get_format(),
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    ret.add_loading_event(end_command_buffer(*dev, cmd));

    return ret;
}

}
