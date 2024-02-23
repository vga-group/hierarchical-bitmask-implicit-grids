#include "window.hh"
#include "vulkan_helpers.hh"
#include "context.hh"
#include "core/error.hh"
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <numeric>

namespace rb::gfx
{

window::window(
    context& ctx,
    const std::string& title,
    ivec2 size,
    bool fullscreen,
    bool vsync,
    bool grab_mouse,
    int display
):  video_output(ctx), dev(nullptr), title(title), size(size),
    fullscreen(fullscreen), vsync(vsync), grab_mouse(grab_mouse),
    need_swapchain_reset(false)
{
    init_sdl(grab_mouse);
    select_device();
    init_swapchain();
}

window::~window()
{
    deinit_swapchain();
    deinit_sdl();
}

SDL_Window* window::get_window() const
{
    return win;
}

device& window::get_device() const
{
    return *dev;
}

bool window::begin_frame(event& e)
{
    if(need_swapchain_reset)
        return true;

    dev->begin_frame();

    // This is the binary semaphore we will be using
    VkSemaphore sem = binary_start_semaphores[dev->get_in_flight_index()];

    // Get next swapchain image index
    VkResult res = vkAcquireNextImageKHR(
        dev->logical_device, swapchain, UINT64_MAX, sem, VK_NULL_HANDLE,
        &image_index
    );
    if(res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
    {
        need_swapchain_reset = true;
        return true;
    }

    // Convert the binary semaphore into a timeline semaphore
    e = dev->queue_graphics(event{sem, 0}, {}, false);

    return false;
}

bool window::end_frame(event e)
{
    dev->end_frame(e);
    VkSemaphore sem = binary_finish_semaphores[dev->get_in_flight_index()];

    // Convert the input timeline semaphore into a binary semaphore
    dev->convert_timeline_to_binary_semaphore(e, sem);

    // Present!
    VkPresentInfoKHR present_info = {
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        nullptr,
        1, &sem,
        1, &swapchain, &image_index,
        nullptr
    };

    return dev->present(present_info);
}

void window::reset_swapchain()
{
    deinit_swapchain();
    dev->finish();
    init_swapchain();
}

uint32_t window::get_image_index() const
{
    return image_index;
}

uint32_t window::get_image_count() const
{
    return swapchain_images.size();
}

size_t window::get_viewport_count() const
{
    return 1;
}

render_target window::get_render_target(size_t image_index, size_t) const
{
    return render_target(
        swapchain_images[image_index],
        *swapchain_image_views[image_index],
        VK_IMAGE_LAYOUT_UNDEFINED,
        uvec2(size), 0, 1,
        VK_SAMPLE_COUNT_1_BIT,
        surface_format.format
    );
}

void window::set_size(ivec2 size)
{
    SDL_SetWindowSize(win, size.x, size.y);
    need_swapchain_reset = true;
}

ivec2 window::get_size(size_t) const
{
    return size;
}

ivec2 window::get_image_origin(size_t) const
{
    return ivec2(0);
}

VkImageLayout window::get_output_layout(size_t) const
{
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

int window::get_available_displays() const
{
    return SDL_GetNumVideoDisplays();
}

void window::set_current_display(int display)
{
    int cur_display = get_current_display();
    if(cur_display == display || display == -1 || cur_display == -1)
        return;

    // Yes, I know this is really crappy... But it's the only way I got it to
    // work right... You are welcome to improve it. The delays were needed to
    // avoid tripping X11.
    if(fullscreen) SDL_SetWindowFullscreen(win, 0);
    SDL_Delay(100);
    SDL_SetWindowPosition(
        win,
        SDL_WINDOWPOS_CENTERED_DISPLAY(display),
        SDL_WINDOWPOS_CENTERED_DISPLAY(display)
    );
    SDL_Delay(100);
    if(fullscreen)
    {
        SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_GetWindowSize(win, &size.x, &size.y);
    }
}

int window::get_current_display() const
{
    uint32_t flags = SDL_GetWindowFlags(win);
    if(!(flags & SDL_WINDOW_FULLSCREEN_DESKTOP)) return -1;
    return SDL_GetWindowDisplayIndex(win);
}

void window::set_fullscreen(bool fullscreen)
{
    if(this->fullscreen == fullscreen) return;

    SDL_SetWindowFullscreen(win, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

    SDL_GetWindowSize(win, &size.x, &size.y);

    this->fullscreen = fullscreen;
    need_swapchain_reset = true;
}

bool window::is_fullscreen() const
{
    return fullscreen;
}

void window::set_vsync(bool vsync)
{
    this->vsync = vsync;
    need_swapchain_reset = true;
}

bool window::get_vsync() const
{
    return vsync;
}

void window::set_mouse_grab(bool grab_mouse)
{
    this->grab_mouse = grab_mouse;
    SDL_SetWindowGrab(win, (SDL_bool)grab_mouse);
    SDL_SetRelativeMouseMode((SDL_bool)grab_mouse);
}

bool window::is_mouse_grabbed() const
{
    return grab_mouse;
}

void window::select_device()
{
    uint64_t score = 0;
    dev = nullptr;
    for(size_t i = 0; i < get_context().get_device_count(); ++i)
    {
        device* d = get_context().get_device(i);
        if(!d->supports_required_extensions || !d->supports(surface))
            continue;

        uint64_t own_score = 0;
        if(d->supports_ray_tracing_pipeline)
            own_score |= 1<<8;

        switch(d->physical_device_props.properties.deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            own_score |= 1<<4;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            own_score |= 1<<3;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            own_score |= 1<<2;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            own_score |= 1<<1;
            break;
        default:
            break;
        }

        if(own_score > score || score == 0)
        {
            dev = d;
            score = own_score;
        }
    }

    if(!dev)
        RB_PANIC(
            "Unable to find a device with required extensions and ability to "
            "draw to window!"
        );

    dev->open(surface);
}

void window::init_sdl(int display)
{
    win = SDL_CreateWindow(
        title.c_str(),
        display >= 0 ? SDL_WINDOWPOS_CENTERED_DISPLAY(display) : SDL_WINDOWPOS_UNDEFINED,
        display >= 0 ? SDL_WINDOWPOS_CENTERED_DISPLAY(display) : SDL_WINDOWPOS_UNDEFINED,
        size.x,
        size.y,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0)
    );
    if(!win)
        RB_PANIC(SDL_GetError());

    SDL_GetWindowSize(win, &size.x, &size.y);
    set_mouse_grab(grab_mouse);

    if(!SDL_Vulkan_CreateSurface(win, get_context().get_instance(), &surface))
        RB_PANIC(SDL_GetError());
}

void window::deinit_sdl()
{
    vkDestroySurfaceKHR(get_context().get_instance(), surface, nullptr);
    SDL_DestroyWindow(win);
}

void window::init_swapchain()
{
    // Find the format we want
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev->physical_device, surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev->physical_device, surface, &format_count, formats.data());

    bool found_format = false;
    for(VkSurfaceFormatKHR format: formats)
    {
        if(
            (format.format == VK_FORMAT_B8G8R8A8_UNORM ||
            format.format == VK_FORMAT_R8G8B8A8_UNORM) &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        ){
            surface_format = format;
            found_format = true;
            break;
        }
    }

    if(!found_format)
        surface_format = formats[0];

    // Find the presentation mode to use
    uint32_t mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev->physical_device, surface, &mode_count, nullptr);
    std::vector<VkPresentModeKHR> modes(mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev->physical_device, surface, &mode_count, modes.data());

    bool found_mode = false;
    std::vector<VkPresentModeKHR> preferred_modes;
    if(vsync)
    {
        preferred_modes.push_back(VK_PRESENT_MODE_MAILBOX_KHR);
        preferred_modes.push_back(VK_PRESENT_MODE_FIFO_KHR);
    }
    preferred_modes.push_back(VK_PRESENT_MODE_IMMEDIATE_KHR);
    for(VkPresentModeKHR mode: preferred_modes)
    {
        if(std::count(modes.begin(), modes.end(), mode))
        {
            present_mode = mode;
            found_mode = true;
            break;
        }
    }

    if(!found_mode)
        present_mode = modes[0];

    // Get surface capabilities
    VkSurfaceCapabilitiesKHR surface_caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev->physical_device, surface, &surface_caps);

    // Extent check
    size.x = clamp((uint32_t)size.x, surface_caps.minImageExtent.width, surface_caps.maxImageExtent.width);
    size.y = clamp((uint32_t)size.y, surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height);

    uint32_t image_count = std::max(
        dev->get_in_flight_count(),
        surface_caps.minImageCount
    );
    if(surface_caps.maxImageCount != 0)
        image_count = std::min(image_count, surface_caps.maxImageCount);

    // Create the swapchain!
    VkSwapchainCreateInfoKHR swapchain_info = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        nullptr,
        {},
        surface,
        image_count,
        surface_format.format,
        surface_format.colorSpace,
        {(uint32_t)size.x, (uint32_t)size.y},
        1,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|
        VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        1, (const uint32_t*)&dev->graphics_family_index,
        surface_caps.currentTransform,
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        present_mode,
        VK_TRUE,
        {}
    };
    vkCreateSwapchainKHR(dev->logical_device, &swapchain_info, nullptr, &swapchain);

    // Get swapchain images
    vkGetSwapchainImagesKHR(dev->logical_device, swapchain, &image_count, nullptr);
    swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(dev->logical_device, swapchain, &image_count, swapchain_images.data());

    vkres<VkCommandBuffer> cmd = begin_command_buffer(*dev);
    for(VkImage img: swapchain_images)
    {
        image_barrier(
            cmd, img, surface_format.format, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        );
        swapchain_image_views.push_back(create_image_view(*dev, img, surface_format.format, VK_IMAGE_ASPECT_COLOR_BIT));
    }
    end_command_buffer(*dev, cmd).wait(*dev);

    for(size_t i = 0; i < dev->get_in_flight_count(); ++i)
    {
        binary_start_semaphores.push_back(create_binary_semaphore(*dev));
        binary_finish_semaphores.push_back(create_binary_semaphore(*dev));
    }

    need_swapchain_reset = false;
}

void window::deinit_swapchain()
{
    swapchain_image_views.clear();
    dev->finish();

    for(VkSemaphore& sem: binary_start_semaphores)
        vkDestroySemaphore(dev->logical_device, sem, nullptr);
    for(VkSemaphore& sem: binary_finish_semaphores)
        vkDestroySemaphore(dev->logical_device, sem, nullptr);
    binary_start_semaphores.clear();
    binary_finish_semaphores.clear();
    vkDestroySwapchainKHR(dev->logical_device, swapchain, nullptr);
}

}
