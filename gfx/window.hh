#ifndef RAYBASE_GFX_WINDOW_HH
#define RAYBASE_GFX_WINDOW_HH

#include "video_output.hh"
#include "device.hh"
#include "core/math.hh"
#include "render_target.hh"
#include "vkres.hh"
#include <SDL.h>
#include <SDL_vulkan.h>

namespace rb::gfx
{

class window: public video_output
{
public:
    window(
        context& ctx,
        const std::string& title = "Raybase",
        ivec2 size = ivec2(1280, 720),
        bool fullscreen = false,
        bool vsync = true,
        bool grab_mouse = false,
        int display = -1
    );
    ~window();

    SDL_Window* get_window() const;
    device& get_device() const override;

    // Returns true when command buffers referring to the swapchain must be
    // reset.
    bool begin_frame(event& e) override;
    // Returns true when command buffers referring to the swapchain must be
    // reset.
    bool end_frame(event e) override;

    void reset_swapchain() override;

    uint32_t get_image_index() const override;
    uint32_t get_image_count() const override;

    size_t get_viewport_count() const override;
    render_target get_render_target(
        size_t image_index,
        size_t viewport = 0
    ) const override;
    void set_size(ivec2 size);
    ivec2 get_size(size_t viewport = 0) const override;
    ivec2 get_image_origin(size_t viewport = 0) const override;
    VkImageLayout get_output_layout(size_t viewport = 0) const override;

    int get_available_displays() const;

    void set_current_display(int display = -1);
    int get_current_display() const;

    void set_fullscreen(bool fullscreen);
    bool is_fullscreen() const;

    void set_vsync(bool vsync);
    bool get_vsync() const;

    void set_mouse_grab(bool grab_mouse);
    bool is_mouse_grabbed() const;

private:
    void select_device();

    void init_sdl(int display);
    void deinit_sdl();

    void init_swapchain();
    void deinit_swapchain();

    device* dev;

    // SDL-related members
    std::string title;
    ivec2 size;
    bool fullscreen;
    bool vsync;
    bool grab_mouse;
    bool need_swapchain_reset;
    SDL_Window* win;

    // Vulkan-related members
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;

    // Swapchain resources
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchain_images;
    std::vector<vkres<VkImageView>> swapchain_image_views;
    std::vector<VkSemaphore> binary_start_semaphores;
    std::vector<VkSemaphore> binary_finish_semaphores;
    uint32_t image_index;
};

}

#endif

