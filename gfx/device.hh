#ifndef RAYBASE_GFX_DEVICE_HH
#define RAYBASE_GFX_DEVICE_HH

#include "volk.h"
#include "vk_mem_alloc.h"
#include "garbage_collector.hh"
#include "core/types.hh"
#include "core/argvec.hh"
#include <vector>
#include <chrono>
#include <unordered_map>
#include <string>
#include <thread>
#include <mutex>
#include <shared_mutex>

namespace rb::gfx
{

struct event;
class context;
class device
{
friend class context;
public:
    device(
        context* ctx,
        VkInstance instance,
        VkPhysicalDevice device,
        const std::vector<const char*>& validation_layers
    );
    device(device& other) = delete;
    device(device&& other) = delete;
    ~device();

    bool supports(VkSurfaceKHR surface) const;
    void open(VkSurfaceKHR surface);
    bool is_open() const;
    void close();
    void finish();

    void begin_frame();
    void end_frame(const event& e);

    uint32_t get_in_flight_index() const;
    uint32_t get_in_flight_count() const;
    void set_in_flight_count(uint32_t count);

    uint64_t get_frame_counter() const;
    time_ticks get_frame_time() const;

    VkQueryPool get_timestamp_query_pool(uint32_t frame_index);
    int32_t add_timer(const std::string& name);
    void remove_timer(uint32_t frame_index);
    void dump_timing() const;
    std::vector<std::pair<std::string, double>> get_timing_results() const;

    void start_async_load(
        VkSemaphoreSubmitInfoKHR& wait_info,
        VkSemaphoreSubmitInfoKHR& signal_info
    );
    void finish_async_load(
        VkSemaphoreSubmitInfoKHR& wait_info
    ) const;
    bool is_async_load_finished() const;

    event queue_graphics(
        argvec<event> wait_for,
        argvec<VkCommandBuffer> command_buffers,
        bool async_load = false
    );
    event queue_compute(
        argvec<event> wait_for,
        argvec<VkCommandBuffer> command_buffers,
        bool async_load = false
    );
    event queue_transfer(
        argvec<event> wait_for,
        argvec<VkCommandBuffer> command_buffers,
        bool async_load = false
    );
    // Please use the queue_*() functions instead of dealing with the mutex
    // manually. These functions exist only as an escape hatch when interfacing
    // with libraries.
    std::mutex& get_graphics_queue_mutex(bool async_load);
    std::mutex& get_compute_queue_mutex(bool async_load);
    std::mutex& get_transfer_queue_mutex(bool async_load);

    void convert_timeline_to_binary_semaphore(
        event wait_for,
        VkSemaphore binary_signal
    );

    event get_graphics_async_load_event() const;
    event get_compute_async_load_event() const;
    event get_transfer_async_load_event() const;

    event get_next_graphics_frame_event(size_t i = 0) const;
    event get_next_compute_frame_event(size_t i = 0) const;
    event get_next_transfer_frame_event(size_t i = 0) const;

    VkCommandPool get_graphics_pool() const;
    VkCommandPool get_compute_pool() const;
    VkCommandPool get_transfer_pool() const;


    // Command buffers are super annoying hazardous waste, because they have to
    // be released on the same thread that they were allocated on. So the
    // regular garbage collector can't cleanly deal with them on its own.
    void add_command_buffer_garbage(VkCommandPool pool, VkCommandBuffer buf);

    // You should allocate your command buffers with this call, which makes sure
    // that used command buffers get released properly.
    VkCommandBuffer allocate_command_buffer(VkCommandPool pool);

    bool present(VkPresentInfoKHR& present);
    VkQueue get_graphics_queue(bool async_load = false);

    context* ctx;

    bool supports_ray_tracing_pipeline;
    bool supports_ray_query;
    bool supports_conservative_rasterization;
    bool supports_required_extensions;
    unsigned max_multi_views;

    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    VkPhysicalDeviceProperties2 physical_device_props;
    VkPhysicalDeviceFeatures2 physical_device_features;
    VkPhysicalDeviceVulkan11Features vulkan11_features;
    VkPhysicalDeviceVulkan11Properties vulkan11_props;
    VkPhysicalDeviceVulkan12Features vulkan12_features;
    VkPhysicalDeviceSynchronization2FeaturesKHR sync2_features;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rp_features;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rp_properties;
    VkPhysicalDeviceRayQueryFeaturesKHR rq_features;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR as_features;
    VkPhysicalDeviceAccelerationStructurePropertiesKHR as_properties;
    VkPhysicalDeviceConservativeRasterizationPropertiesEXT cr_properties;
    VkPhysicalDeviceSubgroupProperties subgroup_properties;
    std::vector<VkQueueFamilyProperties> queue_families;
    int32_t compute_family_index;
    int32_t graphics_family_index;
    int32_t transfer_family_index;
    VmaAllocator allocator;

    VkSampleCountFlags available_sample_counts;

    // Resource management
    garbage_collector gc;

    // Pipelines
    VkPipelineCache pp_cache;

private:
    void queue_inner(
        event e,
        VkQueue queue,
        argvec<event> wait_for,
        argvec<VkCommandBuffer> command_buffers
    );
    void queue_dependencies(VkSubmitInfo2KHR& submit);
    VkCommandPool get_pool_inner(
        uint32_t family_index,
        std::unordered_map<std::thread::id, VkCommandPool>& pools
    ) const;
    void release_pool_garbage(VkCommandPool pool);
    void select_queue_families(VkSurfaceKHR surface);
    void open();
    void init_timing();
    void deinit_timing();
    void update_timing_results();

    VkInstance instance;
    std::vector<const char*> validation_layers;
    std::vector<const char*> device_extensions;

    // Timing resources
    std::vector<VkQueryPool> timestamp_query_pools;
    std::vector<int32_t> free_queries;
    std::unordered_map<int32_t, std::string> timers;
    std::chrono::steady_clock::duration cpu_frame_duration;
    std::chrono::steady_clock::duration cpu_wait_duration;
    std::chrono::steady_clock::time_point cpu_frame_start_time;
    std::vector<std::pair<std::string, double>> timing_results;

    // Frame counting
    VkSemaphore frame_finish_semaphore; // used to limit in-flight frames
    uint32_t in_flight_count;
    uint32_t in_flight_index;
    uint64_t frame_counter;

    // Command pools
    mutable std::shared_mutex command_pool_mutex;
    mutable std::unordered_map<std::thread::id, VkCommandPool> graphics_pools;
    mutable std::unordered_map<std::thread::id, VkCommandPool> compute_pools;
    mutable std::unordered_map<std::thread::id, VkCommandPool> transfer_pools;
    mutable std::unordered_map<VkCommandPool, std::vector<VkCommandBuffer>> command_pool_garbage;

    // Queues
    VkQueue graphics_queue[2]; // 0: per-frame, 1: async
    VkSemaphore graphics_semaphores[2];
    std::mutex graphics_mutexes[2];
    uint64_t graphics_semaphore_values[2];

    VkQueue compute_queue[2]; // 0: per-frame, 1: async
    VkSemaphore compute_semaphores[2];
    std::mutex compute_mutexes[2];
    uint64_t compute_semaphore_values[2];

    VkQueue transfer_queue[2]; // 0: per-frame, 1: async
    VkSemaphore transfer_semaphores[2];
    std::mutex transfer_mutexes[2];
    uint64_t transfer_semaphore_values[2];
};

}

#endif
