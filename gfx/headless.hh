#ifndef RAYBASE_GFX_HEADLESS_HH
#define RAYBASE_GFX_HEADLESS_HH

#include "video_output.hh"
#include "device.hh"
#include "core/math.hh"
#include "render_target.hh"
#include "vkres.hh"

namespace rb::gfx
{

// A headless video output. If a pixel_data pointer is given, it only copies
// data to it, so there's no graphical output. Not the smartest so far, it
// doesn't really support in-flight frames and syncs on end_frame.
class headless: public video_output
{
public:
    // The default parameters create a truly headless context that does
    // absolutely nothing other than deal with begin_frame and end_frame. But
    // at that point, you could just call device->begin_frame() and end_frame()
    // yourself..
    headless(
        context& ctx,
        ivec2 size = ivec2(0),
        void* pixel_data = 0,
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM
    );
    ~headless();

    device& get_device() const override;

    bool begin_frame(event& e) override;

    // Flushes changes to pixel_data.
    bool end_frame(event e) override;

    void reset_swapchain() override;

    uint32_t get_image_index() const override;
    uint32_t get_image_count() const override;

    size_t get_viewport_count() const override;
    render_target get_render_target(
        size_t image_index,
        size_t viewport = 0
    ) const override;
    ivec2 get_size(size_t viewport = 0) const override;
    ivec2 get_image_origin(size_t viewport = 0) const override;
    VkImageLayout get_output_layout(size_t viewport = 0) const override;

private:
    void select_device();

    void init_swapchain();
    void deinit_swapchain();

    device* dev;

    // SDL-related members
    ivec2 size;
    void* pixel_data;
    VkFormat format;
    size_t bytes;

    // Vulkan-related members
    vkres<VkImage> render_image;
    vkres<VkImageView> render_view;
    vkres<VkBuffer> read_buf;
    vkres<VkCommandBuffer> copy_cmd;
};

}

#endif

