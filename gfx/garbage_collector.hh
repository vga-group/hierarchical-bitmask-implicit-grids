#ifndef RAYBASE_GFX_GARBAGE_COLLECTOR_HH
#define RAYBASE_GFX_GARBAGE_COLLECTOR_HH

#include "volk.h"
#include <functional>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include "core/argvec.hh"
#include "core/log.hh"
//#define RAYBASE_GFX_GARBAGE_COLLECTOR_DEBUG

#ifdef RAYBASE_GFX_GARBAGE_COLLECTOR_DEBUG
#define RB_GC_LABEL(gc, resource, ...) gc.add_label(resource, __LINE__, __FILENAME__, __VA_ARGS__)
#else
#define RB_GC_LABEL(gc, resource, ...)
#endif

namespace rb::gfx
{

class device;

// This can be used to figure out which deletion causes issues.
//extern int64_t sync_trap_counter;

// A thread-safe garbage collector for Vulkan resources, particularly for the
// GPU side of things. It handles the synchronization basically perfectly, as
// long as it's properly informed.
class garbage_collector
{
public:
    garbage_collector(device& dev);
    garbage_collector(const garbage_collector&) = delete;
    garbage_collector(garbage_collector&& other) noexcept = delete;
    ~garbage_collector();

    void remove(void* resource, std::function<void()>&& cleanup);
#ifdef RAYBASE_GFX_GARBAGE_COLLECTOR_DEBUG
    template<typename T>
    void remove(T* resource, std::function<void()>&& cleanup)
    {
        add_default_label<T>(resource);
        remove((void*)resource, std::move(cleanup));
    }
#endif
    void remove(VkSemaphore sem);

    // Checks all known semaphores, ensuring that they are ready for deletion.
    void collect();

    // Waits until all resources are collectable, then collects them.
    void wait_collect();

    // The callback is called once the semaphore hits the given value.
    void add_trigger(
        VkSemaphore timeline,
        uint64_t value,
        std::function<void()>&& callback
    );

    // The 'user' must be deleted before 'used'.
    void depend(void* used_resource, void* user_resource);
    // Faster for depending on many things simultaneously.
    void depend_many(argvec<void*> used_resource, void* user_resource);
    void depend(void* used_resource, VkSemaphore timeline, uint64_t value);

#ifdef RAYBASE_GFX_GARBAGE_COLLECTOR_DEBUG
    template<typename... Args>
    void add_label(const void* resource, int line, const char* file, const Args&... rest)
    {
        debug_infos[resource] = std::string("(")+file+":"+std::to_string(line)+"): " + make_string(rest...);
    }

    template<typename T>
    void add_default_label(const void* resource)
    {
        if(debug_infos.count(resource) == 0)
            debug_infos[resource] = std::string("unannotated ") + typeid(T*).name();
    }

    template<typename T, typename U>
    void depend(T* used_resource, U* user_resource)
    {
        add_default_label<T>(used_resource);
        add_default_label<U>(user_resource);
        depend((void*)used_resource, (void*)user_resource);
    }

    template<typename T, typename U>
    void depend_many(argvec<T*> used_resource, U* user_resource)
    {
        for(T* res: used_resource)
            add_default_label<T>(res);
        add_default_label<U>(user_resource);
        depend_many(used_resource.make_void_ptr(), (void*)user_resource);
    }

    template<typename T>
    void depend(T* used_resource, VkSemaphore timeline, uint64_t value)
    {
        add_default_label<T>(used_resource);
        depend((void*)used_resource, timeline, value);
    }
#endif

private:
    void depend_impl(argvec<void*> used_resource, void* user_resource);
    void depend_impl(void* used_resource, VkSemaphore timeline, uint64_t value);
    void check_delete(void* resource);

    std::mutex mutex;
    device* dev;

    struct dependency_info
    {
        size_t dependency_count = 0;
        std::vector<void* /*resource*/> dependents;
        std::function<void()> cleanup;
    };

    std::unordered_map<void* /*resource*/, dependency_info> resources;

    struct trigger
    {
        uint64_t value;
        void* dependent;
        std::function<void()> callback;
        bool operator<(const trigger& t) const;
    };

    struct semaphore_info
    {
        std::priority_queue<trigger> triggers;
        bool should_destroy = false;
    };
    std::unordered_map<VkSemaphore, semaphore_info> semaphore_dependencies;

#ifdef RAYBASE_GFX_GARBAGE_COLLECTOR_DEBUG
    std::unordered_map<const void*, std::string> debug_infos;
#endif
};

}

#endif

