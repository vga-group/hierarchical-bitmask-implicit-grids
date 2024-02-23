#include "context.hh"
#include "core/input.hh"
#include "core/error.hh"
#ifdef RAYBASE_HAS_SDL2
#include <SDL.h>
#include <SDL_vulkan.h>
#endif
#include <iostream>
#include <cstring>

namespace rb::gfx
{

context::context(thread_pool* pool)
:
#ifdef RAYBASE_HAS_SDL2
    sdl(SDL_INIT_VIDEO),
#endif
    threads(pool)
{
    // Create thread pool if not shared.
    if(!threads)
    {
        owned_threads.reset(new thread_pool);
        threads = owned_threads.get();
    }

    // Set the initial time for logging
    get_initial_time();

#ifdef RAYBASE_HAS_SDL2
    // Start SDL & Vulkan
    SDL_Vulkan_LoadLibrary(nullptr);

    unsigned count = 0;
    if(!SDL_Vulkan_GetInstanceExtensions(nullptr, &count, nullptr))
        RB_PANIC("SDL_Vulkan_GetInstanceExtensions: ", SDL_GetError());

    extensions.resize(count);
    if(!SDL_Vulkan_GetInstanceExtensions(nullptr, &count, extensions.data()))
        RB_PANIC("SDL_Vulkan_GetInstanceExtensions: ", SDL_GetError());
#endif
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    init_vulkan();

    uint32_t physical_device_count = 0;
    vkEnumeratePhysicalDevices(vulkan, &physical_device_count, nullptr);
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vkEnumeratePhysicalDevices(vulkan, &physical_device_count, physical_devices.data());

    for(VkPhysicalDevice d: physical_devices)
        devices.emplace_back(new device(this, vulkan, d, validation_layers));
}

context::~context()
{
    // There may be some pending graphics tasks, so ensure that those are
    // finished.
    for(auto& dev: devices)
        dev->gc.wait_collect();
    threads->finish_all_existing();

    devices.clear();
    deinit_vulkan();
}

VkInstance context::get_instance() const
{
    return vulkan;
}

size_t context::get_device_count() const
{
    return devices.size();
}

device* context::get_device(size_t i)
{
    return devices[i].get();
}

const device* context::get_device(size_t i) const
{
    return devices[i].get();
}

thread_pool& context::get_thread_pool() const
{
    return *threads;
}

void context::init_vulkan()
{
    if(volkInitialize() != VK_SUCCESS)
        RB_PANIC("volk");

    VkApplicationInfo app_info {
        VK_STRUCTURE_TYPE_APPLICATION_INFO,
        nullptr,
        "Raybase",
        VK_MAKE_VERSION(1,0,0),
        "Raybase",
        VK_MAKE_VERSION(1,0,0),
        VK_API_VERSION_1_2
    };

    uint32_t available_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&available_layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(available_layer_count);
    vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers.data());
#ifndef NDEBUG
    for(auto& layer: available_layers)
    {
        if(strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0)
        {
            validation_layers.push_back("VK_LAYER_KHRONOS_validation");
        }
    }

    VkValidationFeatureEnableEXT enabled_features[] = {VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT};
    VkValidationFeaturesEXT features = {
        VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
        nullptr,
        1,
        enabled_features,
        0,
        nullptr
    };
#endif

    VkInstanceCreateInfo instance_info {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        nullptr,
        0,
        &app_info,
        (uint32_t)validation_layers.size(), validation_layers.data(),
        (uint32_t)extensions.size(), extensions.data()
    };
#ifndef NDEBUG
    instance_info.pNext = &features;
#endif

    VkResult res = vkCreateInstance(&instance_info, nullptr, &vulkan);
    if(res != VK_SUCCESS)
        RB_PANIC("vkCreateInstance ", res);

    volkLoadInstance(vulkan);

#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT messenger_info = {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        nullptr,
        0,
        //VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT|
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT|
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        [](
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT* data,
            void*
        ) -> VkBool32 {
            if((severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
            {
                RB_PANIC(data->pMessage);
            }
            else
            {
                RB_LOG(data->pMessage);
            }
            return false;
        },
        nullptr
    };
    vkCreateDebugUtilsMessengerEXT(vulkan, &messenger_info, nullptr, &messenger);
#endif
}

void context::deinit_vulkan()
{
#ifndef NDEBUG
    vkDestroyDebugUtilsMessengerEXT(vulkan, messenger, nullptr);
#endif
    vkDestroyInstance(vulkan, nullptr);
}

}
