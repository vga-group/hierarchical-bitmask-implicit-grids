#ifndef RAYBASE_GFX_CONTEXT_HH
#define RAYBASE_GFX_CONTEXT_HH

#include "device.hh"
#ifdef RAYBASE_HAS_SDL2
#include "core/sdl.hh"
#endif
#include "core/thread_pool.hh"
#include <memory>

namespace rb::gfx
{

class device;
class context
{
public:
    context(thread_pool* pool = nullptr);
    ~context();

    VkInstance get_instance() const;
    size_t get_device_count() const;
    device* get_device(size_t i);
    const device* get_device(size_t i) const;

    thread_pool& get_thread_pool() const;

private:
    void init_vulkan();
    void deinit_vulkan();

#ifdef RAYBASE_HAS_SDL2
    sdl_requirement sdl;
#endif

    // Vulkan-related members
    VkInstance vulkan;
    VkDebugUtilsMessengerEXT messenger;
    std::vector<const char*> extensions;
    std::vector<const char*> validation_layers;
    std::vector<std::unique_ptr<device>> devices;
    thread_pool* threads;
    std::unique_ptr<thread_pool> owned_threads;
};

}

#endif
