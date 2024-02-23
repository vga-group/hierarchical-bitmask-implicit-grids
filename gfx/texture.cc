#include "texture.hh"
#include "vulkan_helpers.hh"
#include "stb_image.h"
#include "core/io.hh"
#include "core/string.hh"
#include "core/error.hh"

namespace rb::gfx
{

texture::texture(device& dev)
: async_loadable_resource(dev), need_metadata_sync(true)
{
}

texture::texture(
    device& dev,
    const file& f,
    VkImageLayout layout,
    bool mipmapped,
    bool flip_on_load
): async_loadable_resource(dev), need_metadata_sync(true)
{
    super_impl_data* data = &impl(false);
    data->opt.layout = layout;
    async_load([f, mipmapped, data, flip_on_load](){
        load_from_file(*data, f, mipmapped, flip_on_load);
    });
}

texture::texture(
    device& dev,
    const options& opt,
    size_t data_size,
    const void* pixel_data
):  async_loadable_resource(dev), need_metadata_sync(false)
{
    super_impl_data* data = &impl(false);
    data->opt = opt;
    save_pixel_data(*data, data_size, pixel_data);
    async_load([data](){
        load_from_data(*data);
    });
}

texture texture::create_framebuffer(
    device& dev,
    uvec2 size,
    VkFormat format,
    VkSampleCountFlagBits samples,
    VkImageLayout initial_layout,
    bool mipmapped,
    unsigned layer_count
){
    options opt;
    opt.dim = uvec3(size, 1);
    opt.format = format;
    opt.tiling = VK_IMAGE_TILING_OPTIMAL;
    opt.usage = deduce_image_usage_flags(format, true, true);
    opt.layout = initial_layout;
    opt.samples = samples;
    opt.array_layers = layer_count;
    opt.type = layer_count > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    opt.mipmapped = mipmapped;
    return texture(dev, opt, 0, nullptr);
}

texture texture::load_resource(const file& f, device& dev, bool flip_on_load)
{
    return texture(dev, f, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true, flip_on_load);
}

VkImageView texture::get_image_view() const
{
    return impl().view;
}

VkImageView texture::get_layer_image_view(unsigned layer) const
{
    super_impl_data& i = impl();
    create_layer_views(i);
    return layer >= i.layer_views.size() ? *i.view : *i.layer_views[layer];
}

VkImageView texture::get_mipmap_image_view(unsigned level) const
{
    super_impl_data& i = impl();
    create_mipmap_views(i);
    return level >= i.mipmap_views.size() ? *i.view : *i.mipmap_views[level];
}

VkImage texture::get_image() const
{
    return impl().image;
}

render_target texture::get_render_target(int layer) const
{
    super_impl_data& i = impl();
    if(layer >= 0)
        create_layer_views(i);

    return render_target(
        i.image,
        (layer < 0 || i.layer_views.size() == 0 ? *i.view : *i.layer_views[layer]),
        i.opt.layout, i.opt.dim,
        (layer < 0 || i.layer_views.size() == 0 ? 0 : layer),
        (layer < 0 ? get_layer_count() : 1),
        i.opt.samples, i.opt.format
    );
}

VkImageViewType texture::get_view_type() const
{
    return impl(need_metadata_sync).opt.type;
}

unsigned texture::get_layer_count() const
{
    VkImageViewType type = get_view_type();
    return impl(need_metadata_sync).opt.array_layers *
        (type == VK_IMAGE_VIEW_TYPE_CUBE || type == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY ? 6u : 1u);
}

unsigned texture::get_mipmap_level_count() const
{
    impl_data& i = impl(need_metadata_sync);
    return i.opt.mipmapped ? calculate_mipmap_count(uvec2(i.opt.dim)) : 1;
}

VkFormat texture::get_format() const { return impl(need_metadata_sync).opt.format; }
VkSampleCountFlagBits texture::get_samples() const { return impl(need_metadata_sync).opt.samples; }

device& texture::get_device() const
{
    return *dev;
}

void texture::set_opaque(bool opaque) { impl(need_metadata_sync).opt.opaque = opaque; }
bool texture::potentially_transparent() const
{
    switch(impl(need_metadata_sync).opt.format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R64G64B64A64_SFLOAT:
        return !impl(need_metadata_sync).opt.opaque;
    default:
        return false;
    }
}

uvec2 texture::get_size() const { return impl(need_metadata_sync).opt.dim; }
uvec3 texture::get_dim() const { return impl(need_metadata_sync).opt.dim; }
uvec3 texture::get_mipmap_dim(unsigned level) const
{
    impl_data& i = impl(need_metadata_sync);
    uvec3 dim = get_dim();
    if(i.opt.mipmapped == false)
        return level == 0 ? dim : uvec3(0);
    return max(dim >> level, uvec3(1));
}
float texture::get_aspect() const { auto& i = impl(need_metadata_sync); return i.opt.dim.x/(float)i.opt.dim.y; }

texture::options texture::get_create_options() const
{
    return impl(need_metadata_sync).opt;
}

template<typename T>
std::vector<T> texture::read() const
{
    auto& i = impl(need_metadata_sync);
    return read_formatted_data<T>(i.pixel_data.size(), i.pixel_data.data(), i.opt.format);
}

size_t texture::rle_item_size_hint(const options& opt)
{
    return get_format_size(opt.format);
}

void texture::sync_pixel_data() const
{
    auto& i = impl();
    i.dev->finish();

    size_t pixel_size = get_format_size(i.opt.format);
    size_t array_pixel_size = i.opt.array_layers * pixel_size;
    uvec3 dim = i.opt.dim;
    size_t bytes = dim.x * dim.y * dim.z * array_pixel_size;
    size_t mip_levels = 1;

    if(i.opt.mipmapped)
    {
        while(dim != uvec3(1))
        {
            dim = max(dim/2u, uvec3(1));
            mip_levels++;
            bytes += dim.x * dim.y * dim.z * array_pixel_size;
        }
    }

    vkres<VkBuffer> read_buf = create_readback_buffer(*i.dev, bytes);

    vkres<VkCommandBuffer> cmd = begin_command_buffer(*i.dev);

    image_barrier(cmd, i.image, i.opt.format, i.opt.layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    size_t offset = 0;
    std::vector<VkBufferImageCopy> copies;

    dim = i.opt.dim;
    for(uint32_t mip = 0; mip < mip_levels; ++mip)
    {
        for(uint32_t layer = 0; layer < i.opt.array_layers; ++layer)
        {
            copies.push_back({
                offset, 0, 0,
                {VK_IMAGE_ASPECT_COLOR_BIT, mip, layer, 1},
                {0, 0, 0},
                {dim.x, dim.y, dim.z}
            });

            offset += dim.x * dim.y * dim.z * pixel_size;
        }
        dim = max(dim/2u, uvec3(1));
    }

    vkCmdCopyImageToBuffer(
        cmd,
        i.image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        *read_buf, copies.size(), copies.data()
    );
    image_barrier(cmd, i.image, i.opt.format, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, i.opt.layout);

    vkEndCommandBuffer(cmd);
    event res = i.dev->queue_graphics({}, *cmd, true);
    res.wait(*i.dev);

    i.pixel_data.resize(bytes);

    const VmaAllocator& allocator = dev->allocator;
    void* data;
    vmaMapMemory(allocator, read_buf.get_allocation(), &data);
    memcpy(i.pixel_data.data(), data, bytes);
    vmaUnmapMemory(allocator, read_buf.get_allocation());
}

void texture::save(const std::string& path, bool flip)
{
    fs::path fp(path);
    save_stb(impl(), path, flip);
}

void texture::load_from_file(super_impl_data& d, const file& f, bool mipmapped, bool flip)
{
    fs::path fp(f.get_name());
    load_from_stb(d, f, mipmapped, flip);
}

void texture::load_from_stb(super_impl_data& d, const file& f, bool mipmapped, bool flip)
{
    bool hdr = stbi_is_hdr_from_memory(f.get_data(), f.get_size());
    void* data = nullptr;
    size_t data_size = 0;
    int channels = 0;
    d.opt.dim.z = 1;
    stbi_set_flip_vertically_on_load(flip);
    if(hdr)
    {
        data = stbi_loadf_from_memory(
            f.get_data(), f.get_size(), (int*)&d.opt.dim.x, (int*)&d.opt.dim.y, &channels, 0
        );
        data_size = d.opt.dim.x * d.opt.dim.y * channels * sizeof(float);
    }
    else
    {
        data = stbi_load_from_memory(
            f.get_data(), f.get_size(), (int*)&d.opt.dim.x, (int*)&d.opt.dim.y, &channels, 0
        );
        data_size = d.opt.dim.x * d.opt.dim.y * channels * sizeof(uint8_t);
    }
    RB_CHECK(!data, "Failed to load image ", f.get_name());
    d.opt.opaque = channels < 4;

    // Vulkan implementations don't really support 3-channel textures...
    if(channels == 3)
    {
        if(hdr)
        {
            size_t new_data_size = d.opt.dim.x * d.opt.dim.y * 4 * sizeof(float);
            float* new_data = (float*)malloc(new_data_size);
            float fill = 1.0;
            interlace(new_data, data, &fill, 3 * sizeof(float), 4 * sizeof(float), d.opt.dim.x*d.opt.dim.y);
            free(data);
            data = new_data;
            data_size = new_data_size;
        }
        else
        {
            size_t new_data_size = d.opt.dim.x * d.opt.dim.y * 4 * sizeof(uint8_t);
            uint8_t* new_data = (uint8_t*)malloc(new_data_size);
            uint8_t fill = 255;
            interlace(new_data, data, &fill, 3 * sizeof(uint8_t), 4 * sizeof(uint8_t), d.opt.dim.x*d.opt.dim.y);
            free(data);
            data = new_data;
            data_size = new_data_size;
        }
        channels = 4;
    }

    switch(channels)
    {
    default:
    case 1:
        d.opt.format = hdr ? VK_FORMAT_R32_SFLOAT : VK_FORMAT_R8_UNORM;
        break;
    case 2:
        d.opt.format = hdr ? VK_FORMAT_R32G32_SFLOAT : VK_FORMAT_R8G8_UNORM;
        break;
    case 3:
        d.opt.format = hdr ? VK_FORMAT_R32G32B32_SFLOAT : VK_FORMAT_R8G8B8_UNORM;
        break;
    case 4:
        d.opt.format = hdr ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
        break;
    }

    d.opt.tiling = VK_IMAGE_TILING_OPTIMAL;
    d.opt.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    if(d.opt.layout == VK_IMAGE_LAYOUT_GENERAL)
        d.opt.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    d.opt.samples = VK_SAMPLE_COUNT_1_BIT;
    d.opt.type = VK_IMAGE_VIEW_TYPE_2D;
    d.opt.array_layers = 1;

    d.image = create_gpu_image(
        *d.dev, d.loading_events.emplace_back(), d.opt.dim, 1, d.opt.format,
        d.opt.layout, d.opt.samples, d.opt.tiling, d.opt.usage, VK_IMAGE_VIEW_TYPE_2D,
        data_size, data, mipmapped
    );

    save_pixel_data(d, data_size, data);
    stbi_image_free(data);
    d.view = create_image_view(*d.dev, d.image, d.opt.format, VK_IMAGE_ASPECT_COLOR_BIT);
    RB_GC_LABEL(d.dev->gc, *d.image, d.opt.dim, " ", d.opt.format, " ", d.opt.type, " (loaded from " , f.get_name(), ")");
    RB_GC_LABEL(d.dev->gc, *d.view, d.opt.dim, " ", d.opt.format, " ", d.opt.type, " (loaded from " , f.get_name(), "), view");
}

void texture::load_from_data(super_impl_data& d)
{
    d.image = create_gpu_image(
        *d.dev, d.loading_events.emplace_back(), d.opt.dim, d.opt.array_layers, d.opt.format, d.opt.layout, d.opt.samples,
        d.opt.tiling, d.opt.usage, d.opt.type, d.pixel_data.size(), d.pixel_data.data(), d.opt.mipmapped
    );
    d.view = create_image_view(
        *d.dev, d.image, d.opt.format, deduce_image_aspect_flags(d.opt.format),
        d.opt.type, 0, d.opt.type == VK_IMAGE_VIEW_TYPE_2D ? 1 : VK_REMAINING_ARRAY_LAYERS
    );
    RB_GC_LABEL(d.dev->gc, *d.image, d.opt.dim, " ", d.opt.format, " ", d.opt.type, d.pixel_data.size() == 0 ? " (empty)" : " (loaded from memory)");
    RB_GC_LABEL(d.dev->gc, *d.view, d.opt.dim, " ", d.opt.format, " ", d.opt.type, d.pixel_data.size() == 0 ? " (empty)" : " (loaded from memory), view");
}

void texture::save_pixel_data(impl_data& d, size_t data_size, const void* data)
{
    if(data)
    {
        d.pixel_data.resize(data_size);
        memcpy(d.pixel_data.data(), data, data_size);
    }
}

void texture::save_stb(super_impl_data& d, const std::string& path, bool flip)
{
    RB_CHECK(
        d.opt.type != VK_IMAGE_VIEW_TYPE_2D,
        "Only 2D images can be saved with this file type, but ", path,
        " is not a 2D image. Use .ktx instead."
    );

    d.dev->finish();
    async_save_image(
        *d.dev, path, d.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        d.opt.format, d.opt.dim, {}, flip
    ).wait(*d.dev);
}

void texture::create_layer_views(const super_impl_data& d)
{
    if(d.layer_views.size() == 0)
    {
        size_t layers = d.opt.array_layers;
        if(d.opt.type == VK_IMAGE_VIEW_TYPE_CUBE || d.opt.type == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
            layers *= 6;

        for(unsigned i = 0; i < layers; ++i)
        {
            d.layer_views.push_back(create_image_view(
                *d.dev, d.image, d.opt.format,
                deduce_image_aspect_flags(d.opt.format), d.opt.type, i,
                1, 0, 1
            ));
            RB_GC_LABEL(d.dev->gc, *d.layer_views.back(), " VkImageView (layer view)");
        }
    }
}

void texture::create_mipmap_views(const super_impl_data& d)
{
    if(d.opt.mipmapped && d.mipmap_views.size() == 0)
    {
        unsigned mipmap_count = calculate_mipmap_count(uvec2(d.opt.dim));
        for(unsigned i = 0; i < mipmap_count; ++i)
        {
            d.mipmap_views.push_back(create_image_view(
                *d.dev, d.image, d.opt.format,
                deduce_image_aspect_flags(d.opt.format), d.opt.type, 0,
                VK_REMAINING_ARRAY_LAYERS, i, VK_REMAINING_MIP_LEVELS
            ));
            RB_GC_LABEL(d.dev->gc, *d.mipmap_views.back(), " VkImageView (mipmap view)");
        }
    }
}

template std::vector<float> texture::read() const;
template std::vector<pvec2> texture::read() const;
template std::vector<pvec3> texture::read() const;
template std::vector<pvec4> texture::read() const;
template std::vector<int> texture::read() const;
template std::vector<pivec2> texture::read() const;
template std::vector<pivec3> texture::read() const;
template std::vector<pivec4> texture::read() const;
template std::vector<unsigned> texture::read() const;
template std::vector<puvec2> texture::read() const;
template std::vector<puvec3> texture::read() const;
template std::vector<puvec4> texture::read() const;

}
