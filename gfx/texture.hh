#ifndef RAYBASE_GFX_TEXTURE_HH
#define RAYBASE_GFX_TEXTURE_HH
#include "context.hh"
#include "core/filesystem.hh"
#include "render_target.hh"
#include "resource_loader.hh"
#include "vkres.hh"
#include <map>

namespace rb::gfx
{
class texture;
}

namespace rb
{

// These serializers only do RLE; if you want to save the textures as regular
// image files instead, use the texture::save() function. The notable difference
// is that these work with any format and image type, arrays and mipmaps.
template<typename A>
void save(A& a, const gfx::texture& t);

template<typename A>
bool load(A& a, gfx::texture& t);

}

namespace rb::gfx
{

class texture: public async_loadable_resource<texture>
{
public:
    using metadata = std::map<std::string, std::string>;
    struct options
    {
        uvec3 dim = uvec3(0);
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        unsigned array_layers = 1;
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D;
        bool mipmapped = false;
        bool opaque = false;
    };

    // This constructs an invalid texture that should only ever be moved into.
    texture(device& dev);
    texture(
        device& dev,
        const file& f,
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        bool mipmapped = true,
        bool flip_on_load = true
    );
    texture(
        device& dev,
        const options& opt,
        size_t data_size = 0,
        const void* data = nullptr
    );
    texture(const texture& other) = delete;
    texture(texture&& other) noexcept = default;
    texture& operator=(texture&& other) noexcept = default;

    static texture create_framebuffer(
        device& dev,
        uvec2 size,
        VkFormat format,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        bool mipmapped = false,
        unsigned layer_count = 1
    );

    // for resource_store
    static texture load_resource(
        const file& f,
        device& dev,
        bool flip_on_load = true
    );

    VkImageView get_image_view() const;
    VkImageView get_layer_image_view(unsigned layer) const;
    VkImageView get_mipmap_image_view(unsigned level) const;
    VkImage get_image() const;
    // IMPORTANT: This is layer-faces for cubemaps!
    render_target get_render_target(int layer = -1) const;
    VkImageViewType get_view_type() const;
    unsigned get_layer_count() const;
    unsigned get_mipmap_level_count() const;

    VkFormat get_format() const;
    VkSampleCountFlagBits get_samples() const;

    device& get_device() const;

    void set_opaque(bool opaque);
    bool potentially_transparent() const;

    uvec2 get_size() const;
    uvec3 get_dim() const;
    uvec3 get_mipmap_dim(unsigned level) const;
    float get_aspect() const;

    options get_create_options() const;

    // If you've updated the texture on the GPU before calling read(), you'll
    // need to call sync_pixel_data() first to make this return up-to-date
    // values.
    template<typename T>
    std::vector<T> read() const;
    static size_t rle_item_size_hint(const options& opt);
    void sync_pixel_data() const;

    // This is a slow, blocking function. For screenshots, please use
    // render_pipeline::screenshot() instead, it's non-blocking. Filetype is
    // determined from the path. Flipping only works on non-KTX formats.
    void save(const std::string& path, bool flip = false);

    struct impl_data
    {
        mutable std::vector<uint8_t> pixel_data;
        vkres<VkImage> image;
        vkres<VkImageView> view;
        mutable std::vector<vkres<VkImageView>> layer_views;
        mutable std::vector<vkres<VkImageView>> mipmap_views;
        options opt;
    };

private:
    using super_impl_data = async_loadable_resource<texture>::impl_data;
    static void load_from_file(super_impl_data& d, const file& f, bool mipmapped, bool flip);
    static void load_from_stb(super_impl_data& d, const file& f, bool mipmapped, bool flip);
    static void load_from_data(super_impl_data& d);
    static void save_pixel_data(impl_data& d, size_t data_size, const void* data);
    static void save_stb(super_impl_data& d, const std::string& path, bool flip);
    static void create_layer_views(const super_impl_data& d);
    static void create_mipmap_views(const super_impl_data& d);

    bool need_metadata_sync;

    template<typename A>
    friend void rb::save(A& a, const texture& t);

    template<typename A>
    friend bool rb::load(A& a, texture& t);
};

}

namespace rb
{
template<>
struct resource_loader<gfx::texture>: async_resource_loader<gfx::texture> {};
}

#endif
