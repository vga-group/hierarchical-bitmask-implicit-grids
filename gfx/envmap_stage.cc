#include "envmap_stage.hh"
#include "scene_stage.hh"
#include "envmap.frag.h"
#include "envmap.vert.h"

namespace
{
using namespace rb;
using namespace rb::gfx;

struct parameter_buffer
{
    pmat4 transform;
    pivec2 screen_size;
};

}

namespace rb::gfx
{

envmap_stage::envmap_stage(
    scene_stage& scene,
    render_target& target,
    environment_map* force_envmap
):  render_stage(scene.get_device()),
    target(target),
    force_envmap(force_envmap),
    cubemap(nullptr),
    scene_data(&scene),
    stage_timer(scene.get_device(), "envmap"),
    pipeline(scene.get_device()),
    pass(scene.get_device()),
    descriptors(scene.get_device()),
    parameters(scene.get_device(), sizeof(parameter_buffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
    envmap_sampler(
        scene.get_device(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, 0.0f
    )
{
    render_pass::params pass_params(&target);
    pass.init(pass_params);
    fb.init(pass, {&target});

    raster_shader_data shader;
    shader.fragment = envmap_frag_shader_binary;
    shader.vertex = envmap_vert_shader_binary;
    descriptors.add(shader);

    raster_pipeline::params params(pass, 0, fb.get_size());
    params.rasterization_info.cullMode = VK_CULL_MODE_NONE;

    pipeline.init(params, shader, 0, descriptors.get_layout());
}

envmap_stage::envmap_stage(
    scene_stage& scene,
    render_target& color_target,
    render_target& stencil_target,
    uint32_t stencil_reference,
    environment_map* force_envmap
):  render_stage(scene.get_device()),
    target(color_target),
    force_envmap(force_envmap),
    cubemap(nullptr),
    scene_data(&scene),
    stage_timer(scene.get_device(), "envmap"),
    pipeline(scene.get_device()),
    pass(scene.get_device()),
    descriptors(scene.get_device()),
    parameters(scene.get_device(), sizeof(parameter_buffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
    envmap_sampler(
        scene.get_device(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, 0.0f
    )
{
    render_target* targets[] = {&color_target, &stencil_target};
    render_pass::params pass_params(targets, {});
    pass.init(pass_params);
    fb.init(pass, {&color_target, &stencil_target});

    raster_shader_data shader;
    shader.fragment = envmap_frag_shader_binary;
    shader.vertex = envmap_vert_shader_binary;
    descriptors.add(shader);

    raster_pipeline::params params(pass, 0, fb.get_size());
    params.depth_stencil_info.stencilTestEnable = VK_TRUE;
    VkStencilOpState state = {
        VK_STENCIL_OP_KEEP,
        VK_STENCIL_OP_REPLACE,
        VK_STENCIL_OP_KEEP,
        VK_COMPARE_OP_EQUAL,
        0xFFFFFFFF,
        0,
        stencil_reference
    };
    params.depth_stencil_info.front = state;
    params.depth_stencil_info.back = state;
    params.depth_stencil_info.depthTestEnable = VK_FALSE;
    params.depth_stencil_info.depthWriteEnable = VK_FALSE;

    params.rasterization_info.cullMode = VK_CULL_MODE_NONE;

    pipeline.init(params, shader, 0, descriptors.get_layout());
}

void envmap_stage::set_envmap(environment_map* force_envmap)
{
    this->force_envmap = force_envmap;
}

void envmap_stage::update_buffers(uint32_t frame_index)
{
    scene* s = scene_data->get_scene();

    // Find the camera we want
    const auto& cameras = scene_data->get_active_cameras();
    entity camera_id = cameras[0];
    transformable* cam_transform = s->get<transformable>(camera_id);
    camera* cam = s->get<camera>(camera_id);

    quat orientation = toMat4(cam_transform->get_global_orientation());
    mat4 projection = cam->get_projection();

    // Find the best envmap
    environment_map* envmap = force_envmap;
    if(!envmap)
    {
        entity id = find_sky_envmap(*s);
        envmap = s->get<environment_map>(id);
        if(envmap)
            orientation = orientation * inverse(s->get<transformable>(id)->get_global_orientation());
    }

    parameter_buffer params;
    params.transform = toMat4(orientation) * inverse(projection);
    params.screen_size = target.get_size();
    parameters.update(frame_index, params);

    const texture* new_cubemap = envmap ? envmap->cubemap : nullptr;

    if(cubemap != new_cubemap)
    {
        cubemap = new_cubemap;
        clear_commands();

        descriptors.reset(1);
        descriptors.set_buffer(0, "params", (VkBuffer)parameters);
        if(cubemap)
            descriptors.set_image(0, "envmap", cubemap->get_image_view(), envmap_sampler.get());
        for(uint32_t i = 0; i < dev->get_in_flight_count(); ++i)
        {
            VkCommandBuffer cmd = graphics_commands();
            stage_timer.start(cmd, i);
            upload(cmd, &parameters, i);
            pass.begin(cmd, fb);
            pipeline.bind(cmd);
            pipeline.set_descriptors(cmd, descriptors);

            vkCmdDraw(cmd, 3, 1, 0, 0);

            pass.end(cmd);
            stage_timer.stop(cmd, i);
            use_graphics_commands(cmd, i);
        }
    }

    if(!has_commands())
    {
        for(uint32_t i = 0; i < dev->get_in_flight_count(); ++i)
        {
            VkCommandBuffer cmd = graphics_commands();
            stage_timer.start(cmd, i);
            pass.begin(cmd, fb);
            pass.end(cmd);
            stage_timer.stop(cmd, i);
            use_graphics_commands(cmd, i);
        }
    }
}

}


