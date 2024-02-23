#include "device.hh"
#include "event.hh"
#include "vulkan_helpers.hh"
#include "core/stack_allocator.hh"
#include "core/error.hh"
#include <string>
#ifdef RAYBASE_USE_SDL2
#include <SDL_vulkan.h>
#endif
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <map>
// We need to enable extensions for the radix sort, depending on platform!
#include "radix_sort/radix_sort_vk.h"

namespace
{

bool has_all_extensions(
    const std::vector<VkExtensionProperties>& props,
    const char** extensions,
    size_t extension_count
){
    for(size_t i = 0; i < extension_count; ++i)
    {
        std::string required_name = extensions[i];
        bool found = false;
        for(VkExtensionProperties props: props)
        {
            if(required_name == props.extensionName)
            {
                found = true;
                break;
            }
        }

        if(!found)
            return false;
    }
    return true;
}

const char* base_device_extensions[] = {
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    VK_KHR_MAINTENANCE_4_EXTENSION_NAME
};

const char* rt_device_extensions[] = {
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
};

const char* rq_device_extensions[] = {
    VK_KHR_RAY_QUERY_EXTENSION_NAME
};

const char* bake_device_extensions[] = {
    VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME
};

constexpr uint32_t MAX_TIMER_COUNT = 64;

}

namespace rb::gfx
{

device::device(
    context* ctx,
    VkInstance instance,
    VkPhysicalDevice device,
    const std::vector<const char*>& validation_layers
):  ctx(ctx), physical_device(device), logical_device(VK_NULL_HANDLE),
    gc(*this), pp_cache(VK_NULL_HANDLE), instance(instance),
    validation_layers(validation_layers), cpu_frame_duration({}),
    cpu_wait_duration({}), in_flight_count(2), in_flight_index(0),
    frame_counter(0)
{
    // Check for extensions
    uint32_t available_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &available_count, nullptr);
    std::vector<VkExtensionProperties> extensions(available_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &available_count, extensions.data());

#ifndef NDEBUG
    device_extensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
#endif

    device_extensions.insert(
        device_extensions.end(),
        std::begin(base_device_extensions),
        std::end(base_device_extensions)
    );

    supports_required_extensions = has_all_extensions(extensions, base_device_extensions, std::size(base_device_extensions));
    supports_ray_tracing_pipeline = has_all_extensions(extensions, rt_device_extensions, std::size(rt_device_extensions));
    if(supports_ray_tracing_pipeline)
        device_extensions.insert(device_extensions.end(), std::begin(rt_device_extensions), std::end(rt_device_extensions));
    supports_ray_query = has_all_extensions(extensions, rq_device_extensions, std::size(rq_device_extensions));
    if(supports_ray_query)
        device_extensions.insert(device_extensions.end(), std::begin(rq_device_extensions), std::end(rq_device_extensions));
    supports_conservative_rasterization = has_all_extensions(extensions, bake_device_extensions, std::size(bake_device_extensions));
    if(supports_conservative_rasterization)
        device_extensions.insert(device_extensions.end(), std::begin(bake_device_extensions), std::end(bake_device_extensions));
    max_multi_views = 0;

    bool current_is_discrete = physical_device_props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

    // Find required queue families
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    queue_families.resize(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    // Get properties
    physical_device_props = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &vulkan11_props};
    vulkan11_props = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES, &as_properties};
    as_properties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR, &rp_properties};
    rp_properties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR, &subgroup_properties};
    subgroup_properties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES, &cr_properties};
    cr_properties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT, nullptr};
    vkGetPhysicalDeviceProperties2(device, &physical_device_props);

    // Get features
    physical_device_features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &vulkan11_features};
    vulkan11_features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, &vulkan12_features};
    vulkan12_features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &sync2_features};
    sync2_features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR, nullptr};
    as_features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, nullptr};
    rp_features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, nullptr};
    rq_features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, nullptr};

    void** tail = &sync2_features.pNext;

    if(supports_ray_tracing_pipeline || supports_ray_query)
    {
        *tail = &as_features;
        tail = &as_features.pNext;
    }

    if(supports_ray_tracing_pipeline)
    {
        *tail = &rp_features;
        tail = &rp_features.pNext;
    }

    if(supports_ray_query)
    {
        *tail = &rq_features;
        tail = &rq_features.pNext;
    }

    vkGetPhysicalDeviceFeatures2(physical_device, &physical_device_features);

    physical_device_features.features.samplerAnisotropy = VK_TRUE;
    physical_device_features.features.sampleRateShading = VK_TRUE;
    vulkan12_features.timelineSemaphore = VK_TRUE;
    vulkan12_features.scalarBlockLayout = VK_TRUE;
    sync2_features.synchronization2 = VK_TRUE;
    if(supports_ray_tracing_pipeline)
        vulkan12_features.bufferDeviceAddress = VK_TRUE;
    rq_features.rayQuery = VK_TRUE;
    as_features.accelerationStructure = VK_TRUE;

    if(vulkan11_features.multiview)
        max_multi_views = vulkan11_props.maxMultiviewViewCount;

    // Get the requirements for the radix sort on this platform.
    radix_sort_vk_target_t* rs_target =
        radix_sort_vk_target_auto_detect(&physical_device_props.properties, &subgroup_properties, 2);
    radix_sort_vk_target_requirements_t rs_requirements;
    rs_requirements.ext_name_count = 0;
    rs_requirements.ext_names = nullptr;
    rs_requirements.pdf = &physical_device_features.features;
    rs_requirements.pdf11 = &vulkan11_features;
    rs_requirements.pdf12 = &vulkan12_features;
    RB_CHECK(
        !radix_sort_vk_target_get_requirements(rs_target, &rs_requirements),
        "Radix sort requirement check failed!"
    );
    size_t last_end = device_extensions.size();
    device_extensions.resize(last_end + rs_requirements.ext_name_count);
    rs_requirements.ext_names = device_extensions.data() + last_end;
    RB_CHECK(
        !radix_sort_vk_target_get_requirements(rs_target, &rs_requirements),
        "Radix sort requirement check failed!"
    );
    free(rs_target);

    // Save extra details
    available_sample_counts =
        physical_device_props.properties.limits.framebufferColorSampleCounts &
        physical_device_props.properties.limits.framebufferDepthSampleCounts;
}

device::~device()
{
    close();
}

bool device::supports(VkSurfaceKHR surface) const
{
    for(size_t i = 0; i < queue_families.size(); ++i)
    {
        VkQueueFamilyProperties props = queue_families[i];

        if(props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            VkBool32 has_present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(
                physical_device, i, surface, &has_present
            );

            if(has_present)
                return true;
        }
    }
    return false;
}

void device::open(VkSurfaceKHR surface)
{
    select_queue_families(surface);
    open();
}

void device::open()
{
    if(logical_device) return;

    RB_DBG("Using ", physical_device_props.properties.deviceName);
    RB_DBG("Ray tracing ", supports_ray_tracing_pipeline ? "enabled" : "disabled");
    RB_DBG("Validation layers ", validation_layers.size() > 0 ? "enabled" : "disabled");

    // Create device
    const float priority[] = {1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.5f};
    std::map<int32_t, unsigned> queue_counts;
    queue_counts[graphics_family_index] += 2;
    queue_counts[compute_family_index] += 2;
    queue_counts[transfer_family_index] += 2;

    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    for(auto [family, count]: queue_counts)
    {
        count = min(queue_families[family].queueCount, count);
        queue_infos.push_back({
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, {},
            (uint32_t)family, count, priority
        });
    }

    VkDeviceCreateInfo device_create_info = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        &physical_device_features,
        {},
        (uint32_t)queue_infos.size(), queue_infos.data(),
        (uint32_t)validation_layers.size(), validation_layers.data(),
        (uint32_t)device_extensions.size(), device_extensions.data(),
        nullptr
    };
    VkResult res = vkCreateDevice(physical_device, &device_create_info, nullptr, &logical_device);

    // Get queues
    queue_counts.clear();
    for(size_t i = 0; i < 2; ++i)
    {
        vkGetDeviceQueue(logical_device, graphics_family_index, queue_counts[graphics_family_index], &graphics_queue[i]);
        if(queue_counts[graphics_family_index]+1 < queue_families[graphics_family_index].queueCount)
            queue_counts[graphics_family_index]++;
        graphics_semaphores[i] = create_timeline_semaphore(*this).leak();
        graphics_semaphore_values[i] = 0;
    }
    for(size_t i = 0; i < 2; ++i)
    {
        vkGetDeviceQueue(logical_device, compute_family_index, queue_counts[compute_family_index], &compute_queue[i]);
        if(queue_counts[compute_family_index]+1 < queue_families[compute_family_index].queueCount)
            queue_counts[compute_family_index]++;
        compute_semaphores[i] = create_timeline_semaphore(*this).leak();
        compute_semaphore_values[i] = 0;
    }
    for(size_t i = 0; i < 2; ++i)
    {
        vkGetDeviceQueue(logical_device, transfer_family_index, queue_counts[transfer_family_index], &transfer_queue[i]);
        if(queue_counts[transfer_family_index]+1 < queue_families[transfer_family_index].queueCount)
            queue_counts[transfer_family_index]++;
        transfer_semaphores[i] = create_timeline_semaphore(*this).leak();
        transfer_semaphore_values[i] = 0;
    }

    // Create pipeline cache
    VkPipelineCacheCreateInfo pp_cache_info = {
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        nullptr,
        0,
        0,
        nullptr
    };
    vkCreatePipelineCache(
        logical_device,
        &pp_cache_info,
        nullptr,
        &pp_cache
    );

    // Create memory allocator
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.physicalDevice = physical_device;
    allocator_info.device = logical_device;
    allocator_info.instance = instance;
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    static const VmaVulkanFunctions funcs = {
#ifdef RAYBASE_USE_SDL2
        (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr,
#else
        (PFN_vkGetInstanceProcAddr)vkGetInstanceProcAddr,
#endif
        vkGetDeviceProcAddr,
        vkGetPhysicalDeviceProperties,
        vkGetPhysicalDeviceMemoryProperties
    };
    allocator_info.pVulkanFunctions = &funcs;

    if(supports_ray_tracing_pipeline)
        allocator_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocator_info, &allocator);

    // Create frame semaphores
    frame_finish_semaphore = create_timeline_semaphore(*this).leak();
    init_timing();
}

bool device::is_open() const
{
    return logical_device != VK_NULL_HANDLE;
}

void device::close()
{
    if(is_open())
    {
        finish();
        deinit_timing();
        gc.wait_collect();
        vmaDestroyAllocator(allocator);
        vkDestroySemaphore(logical_device, frame_finish_semaphore, nullptr);
        vkDestroyPipelineCache(logical_device, pp_cache, nullptr);
        pp_cache = VK_NULL_HANDLE;

        for(size_t i = 0; i < 2; ++i)
        {
            vkDestroySemaphore(logical_device, graphics_semaphores[i], nullptr);
            vkDestroySemaphore(logical_device, compute_semaphores[i], nullptr);
            vkDestroySemaphore(logical_device, transfer_semaphores[i], nullptr);
        }
        for(auto [id, pool]: graphics_pools)
            vkDestroyCommandPool(logical_device, pool, nullptr);
        for(auto [id, pool]: compute_pools)
            vkDestroyCommandPool(logical_device, pool, nullptr);
        for(auto [id, pool]: transfer_pools)
            vkDestroyCommandPool(logical_device, pool, nullptr);
        command_pool_garbage.clear();
        vkDestroyDevice(logical_device, nullptr);
    }
    logical_device = VK_NULL_HANDLE;
    frame_counter = 0;
    in_flight_index = 0;
}

void device::finish()
{
    if(is_open())
    {
        VkResult res = vkDeviceWaitIdle(logical_device);
        RB_CHECK(res != VK_SUCCESS, "vkDeviceWaitIdle(): ", res);
        gc.wait_collect();
    }
}

void device::begin_frame()
{
    if(!is_open()) return;

    if(frame_counter > 0)
        in_flight_index = (in_flight_index+1) % in_flight_count;
    frame_counter++;

    if(frame_counter > in_flight_count)
    {
        auto begin_wait = std::chrono::steady_clock::now();
        bool wait_success = wait_timeline_semaphore(
            *this, frame_finish_semaphore, frame_counter - in_flight_count,
            5000000000 // Wait for a second before failing.
        );
        RB_CHECK(
            !wait_success,
            "Likely freeze during rendering. The program cannot safely continue."
        );
        auto end_wait = std::chrono::steady_clock::now();
        gc.collect();
        update_timing_results();
        cpu_wait_duration = end_wait - begin_wait;
    }

    auto cpu_next_start_time = std::chrono::steady_clock::now();
    cpu_frame_duration = cpu_next_start_time - cpu_frame_start_time;
    cpu_frame_start_time = cpu_next_start_time;
}

void device::end_frame(const event& e)
{
    VkSemaphoreSubmitInfoKHR wait_info = {
        VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, nullptr,
        e.timeline_semaphore, e.value, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, 0
    };
    VkSemaphoreSubmitInfoKHR signal_info = {
        VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, nullptr,
        frame_finish_semaphore, frame_counter,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, 0
    };
    VkSubmitInfo2KHR submit_info = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
        nullptr, 0,
        1, &wait_info,
        0, nullptr,
        1, &signal_info
    };
    VkResult res = vkQueueSubmit2KHR(graphics_queue[0], 1, &submit_info, VK_NULL_HANDLE);
    RB_CHECK(res != VK_SUCCESS, "vkQueueSubmit2(): ", res);
}

uint32_t device::get_in_flight_index() const
{
    return in_flight_index;
}

uint32_t device::get_in_flight_count() const
{
    return in_flight_count;
}

void device::set_in_flight_count(uint32_t count)
{
    if(count == in_flight_count) return;

    in_flight_count = count;
    in_flight_index %= in_flight_count;
    if(is_open())
    {
        deinit_timing();
        init_timing();
    }
}

uint64_t device::get_frame_counter() const
{
    return frame_counter;
}

time_ticks device::get_frame_time() const
{
    return (time_ticks)std::chrono::duration_cast<std::chrono::microseconds>(
        cpu_frame_duration
    ).count();
}

VkQueryPool device::get_timestamp_query_pool(uint32_t frame_index)
{
    return timestamp_query_pools[frame_index];
}

int32_t device::add_timer(const std::string& name)
{
    if(free_queries.size() == 0)
    {
        std::cerr << "Failed to get a timer query for " << name << std::endl;
        return -1;
    }
    int32_t index = free_queries.back();
    free_queries.pop_back();
    timers.emplace(index, name);
    return index;
}

void device::remove_timer(uint32_t frame_index)
{
    free_queries.push_back(frame_index);
    timers.erase(frame_index);
}

void device::dump_timing() const
{
    std::cout << "Timing:" << std::endl;
    for(const auto& pair: timing_results)
    {
        std::cout
            << "\t[" << pair.first << "]: "
            << pair.second*1e3 << "ms" << std::endl;
    }
}

std::vector<std::pair<std::string, double>> device::get_timing_results() const
{
    return timing_results;
}

event device::queue_graphics(
    argvec<event> wait_for,
    argvec<VkCommandBuffer> command_buffers,
    bool async_load
){
    unsigned queue_index = async_load ? 1 : 0;

    std::unique_lock lk(graphics_mutexes[queue_index]);
    event e;
    e.timeline_semaphore = graphics_semaphores[queue_index];
    e.value = ++graphics_semaphore_values[queue_index];
    queue_inner(e, graphics_queue[queue_index], wait_for, command_buffers);

    return e;
}

event device::queue_compute(
    argvec<event> wait_for,
    argvec<VkCommandBuffer> command_buffers,
    bool async_load
){
    unsigned queue_index = async_load ? 1 : 0;

    std::unique_lock lk(compute_mutexes[queue_index]);
    event e;
    e.timeline_semaphore = compute_semaphores[queue_index];
    e.value = ++compute_semaphore_values[queue_index];
    queue_inner(e, compute_queue[queue_index], wait_for, command_buffers);

    return e;
}

event device::queue_transfer(
    argvec<event> wait_for,
    argvec<VkCommandBuffer> command_buffers,
    bool async_load
){
    unsigned queue_index = async_load ? 1 : 0;

    std::unique_lock lk(transfer_mutexes[queue_index]);
    event e;
    e.timeline_semaphore = transfer_semaphores[queue_index];
    e.value = ++transfer_semaphore_values[queue_index];
    queue_inner(e, transfer_queue[queue_index], wait_for, command_buffers);

    return e;
}

std::mutex& device::get_graphics_queue_mutex(bool async_load)
{
    return graphics_mutexes[async_load ? 1 : 0];
}

std::mutex& device::get_compute_queue_mutex(bool async_load)
{
    return compute_mutexes[async_load ? 1 : 0];
}

std::mutex& device::get_transfer_queue_mutex(bool async_load)
{
    return transfer_mutexes[async_load ? 1 : 0];
}

void device::convert_timeline_to_binary_semaphore(
    event wait_for,
    VkSemaphore binary_signal
){
    VkSemaphoreSubmitInfoKHR wait_info = {
        VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, nullptr,
        wait_for.timeline_semaphore, wait_for.value,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, 0
    };
    VkSemaphoreSubmitInfoKHR signal_info = {
        VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, nullptr,
        binary_signal, 0, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, 0
    };
    VkSubmitInfo2KHR submit = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
        nullptr, 0,
        1, &wait_info,
        0, nullptr,
        1, &signal_info
    };
    VkResult res = vkQueueSubmit2KHR(
        graphics_queue[0], 1, &submit, VK_NULL_HANDLE
    );
    RB_CHECK(res != VK_SUCCESS, "vkQueueSubmit2(): ", res);
    queue_dependencies(submit);
}

event device::get_graphics_async_load_event() const
{
    return {graphics_semaphores[1], graphics_semaphore_values[1]};
}

event device::get_compute_async_load_event() const
{
    return {compute_semaphores[1], compute_semaphore_values[1]};
}

event device::get_transfer_async_load_event() const
{
    return {transfer_semaphores[1], transfer_semaphore_values[1]};
}

event device::get_next_graphics_frame_event(size_t i) const
{
    return {graphics_semaphores[0], graphics_semaphore_values[0]+1+i};
}

event device::get_next_compute_frame_event(size_t i) const
{
    return {compute_semaphores[0], compute_semaphore_values[0]+1+i};
}

event device::get_next_transfer_frame_event(size_t i) const
{
    return {transfer_semaphores[0], transfer_semaphore_values[0]+1+i};
}

VkCommandPool device::get_graphics_pool() const
{
    return get_pool_inner(graphics_family_index, graphics_pools);
}

VkCommandPool device::get_compute_pool() const
{
    return get_pool_inner(compute_family_index, compute_pools);
}

VkCommandPool device::get_transfer_pool() const
{
    return get_pool_inner(transfer_family_index, transfer_pools);
}

void device::add_command_buffer_garbage(VkCommandPool pool, VkCommandBuffer buf)
{
    std::unique_lock lk(command_pool_mutex);
    command_pool_garbage[pool].push_back(buf);
}

VkCommandBuffer device::allocate_command_buffer(VkCommandPool pool)
{
    release_pool_garbage(pool);

    VkCommandBufferAllocateInfo command_buffer_alloc_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        nullptr,
        pool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        1
    };
    VkCommandBuffer buf;
    vkAllocateCommandBuffers(
        logical_device,
        &command_buffer_alloc_info,
        &buf
    );
    return buf;
}

bool device::present(VkPresentInfoKHR& present)
{
    VkResult res = vkQueuePresentKHR(graphics_queue[0], &present);
    if(res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
        return true;
    return false;
}

VkQueue device::get_graphics_queue(bool async_load)
{
    return graphics_queue[async_load ? 1 : 0];
}

void device::queue_inner(
    event e,
    VkQueue queue,
    argvec<event> wait_for,
    argvec<VkCommandBuffer> command_buffers
){
    auto wait_infos = stack_allocate<VkSemaphoreSubmitInfoKHR>(wait_for.size());
    auto cb_infos = stack_allocate<VkCommandBufferSubmitInfoKHR>(command_buffers.size());
    for(size_t i = 0; i < wait_for.size(); ++i)
    {
        wait_infos[i] = {
            VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, nullptr,
            wait_for[i].timeline_semaphore, wait_for[i].value,
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, 0
        };
    }

    for(size_t i = 0; i < command_buffers.size(); ++i)
    {
        cb_infos[i] = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR, nullptr,
            command_buffers[i], 0
        };
    }

    VkSemaphoreSubmitInfoKHR signal_info = {
        VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR, nullptr,
        e.timeline_semaphore, e.value,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, 0
    };
    VkSubmitInfo2KHR submit = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
        nullptr, 0,
        (uint32_t)wait_infos.size(), wait_infos.get(),
        (uint32_t)cb_infos.size(), cb_infos.get(),
        1, &signal_info
    };
    VkResult res = vkQueueSubmit2KHR(
        queue, 1, &submit, VK_NULL_HANDLE
    );
    RB_CHECK(res != VK_SUCCESS, "vkQueueSubmit2(): ", res);
    queue_dependencies(submit);
}

void device::queue_dependencies(VkSubmitInfo2KHR& submit)
{
    for(size_t i = 0; i < submit.commandBufferInfoCount; ++i)
    {
        for(size_t j = 0; j < submit.signalSemaphoreInfoCount; ++j)
        {
            const VkSemaphoreSubmitInfo& ss = submit.pSignalSemaphoreInfos[j];
            gc.depend(
                submit.pCommandBufferInfos[i].commandBuffer,
                ss.semaphore,
                ss.value
            );
        }
    }
}

VkCommandPool device::get_pool_inner(
    uint32_t family_index,
    std::unordered_map<std::thread::id, VkCommandPool>& pools
) const {
    std::thread::id id = std::this_thread::get_id();
    std::shared_lock lk(command_pool_mutex);
    auto it = pools.find(id);
    if(it == pools.end())
    {
        lk.unlock();
        std::unique_lock ulk(command_pool_mutex);
        VkCommandPool pool;
        VkCommandPoolCreateInfo pool_info = {
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, {},
            (uint32_t)family_index
        };
        vkCreateCommandPool(logical_device, &pool_info, nullptr, &pool);
        it = pools.emplace(id, pool).first;
        return it->second;
    }
    else return it->second;
}

void device::release_pool_garbage(VkCommandPool pool)
{
    std::shared_lock lk(command_pool_mutex);
    auto garbage_it = command_pool_garbage.find(pool);
    if(garbage_it != command_pool_garbage.end())
    {
        if(garbage_it->second.size() > 0)
        {
            lk.unlock();
            std::unique_lock ulk(command_pool_mutex);
            vkFreeCommandBuffers(logical_device, pool, garbage_it->second.size(), garbage_it->second.data());
            garbage_it->second.clear();
        }
    }
}

void device::select_queue_families(VkSurfaceKHR surface)
{
    compute_family_index = -1;
    graphics_family_index  = -1;
    transfer_family_index  = -1;

    // Find graphics queue family first
    for(size_t i = 0; i < queue_families.size(); ++i)
    {
        VkQueueFamilyProperties props = queue_families[i];
        if(props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            VkBool32 has_present = VK_FALSE;
            if(!surface) has_present = VK_TRUE;
            else vkGetPhysicalDeviceSurfaceSupportKHR(
                physical_device, i, surface, &has_present
            );

            if(has_present)
            {
                graphics_family_index = i;
            }
        }
    }

    // Then, try to find a queue family which isn't the graphics queue family
    // for compute.
    for(size_t i = 0; i < queue_families.size(); ++i)
    {
        VkQueueFamilyProperties props = queue_families[i];
        if(props.queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            if(
                compute_family_index < 0 ||
                compute_family_index == graphics_family_index
            ) compute_family_index = i;
        }
    }

    // Finally, attempt to find a transfer queue separate from the previous two.
    for(size_t i = 0; i < queue_families.size(); ++i)
    {
        VkQueueFamilyProperties props = queue_families[i];
        if(props.queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            if(
                transfer_family_index < 0 ||
                transfer_family_index == graphics_family_index ||
                transfer_family_index == compute_family_index
            ) transfer_family_index = i;
        }
    }

    RB_CHECK(graphics_family_index == -1, "Failed to find a graphics queue family");
    RB_CHECK(compute_family_index == -1, "Failed to find a compute queue family");
    RB_CHECK(transfer_family_index == -1, "Failed to find a transfer queue family");
}

void device::init_timing()
{
    timestamp_query_pools.resize(in_flight_count);
    for(uint32_t i = 0; i < in_flight_count; ++i)
    {
        VkQueryPoolCreateInfo info = {
            VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            nullptr,
            {},
            VK_QUERY_TYPE_TIMESTAMP,
            MAX_TIMER_COUNT*2,
            0
        };
        vkCreateQueryPool(
            logical_device,
            &info,
            nullptr,
            &timestamp_query_pools[i]
        );
    }
    free_queries.resize(MAX_TIMER_COUNT);
    std::iota(free_queries.begin(), free_queries.end(), 0u);
}

void device::deinit_timing()
{
    for(VkQueryPool pool: timestamp_query_pools)
        vkDestroyQueryPool(logical_device, pool, nullptr);
    timestamp_query_pools.clear();
    free_queries.clear();
    timers.clear();
}

void device::update_timing_results()
{
    auto results = stack_allocate<uint64_t>(MAX_TIMER_COUNT*2);
    memset(results.get(), 0, sizeof(uint64_t)*MAX_TIMER_COUNT*2);
    vkGetQueryPoolResults(
        logical_device,
        timestamp_query_pools[in_flight_index],
        0, (uint32_t)results.size(),
        results.size()*sizeof(uint64_t), results.get(),
        sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT
    );
    timing_results.clear();

    struct timestamp
    {
        uint64_t start, end;
        std::string_view name;
    };
    auto tmp = stack_allocate<timestamp>(timers.size());

    uint64_t min_start = UINT64_MAX, max_end = 0;
    size_t i = 0;
    for(auto& pair: timers)
    {
        if(results[pair.first*2] == 0)
            continue;
        tmp[i++] = {
            results[pair.first*2],
            results[pair.first*2+1],
            pair.second
        };
        min_start = std::min(min_start, results[pair.first*2]);
        max_end = std::max(max_end, results[pair.first*2+1]);
    }
    std::sort(
        tmp.begin(), tmp.begin() + i,
        [](const timestamp& a, const timestamp& b){
            return a.start < b.start;
        }
    );

    for(size_t j = 0; j < i; ++j)
    {
        timestamp t = tmp[j];
        timing_results.push_back({
            std::string(t.name),
            double(t.end-t.start)*1e-9
        });
    }
}

}
