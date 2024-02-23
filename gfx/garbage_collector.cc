#include "garbage_collector.hh"
#include "core/error.hh"
#include "device.hh"

namespace rb::gfx
{

garbage_collector::garbage_collector(device& dev)
: dev(&dev)
{
}

garbage_collector::~garbage_collector()
{
}

void garbage_collector::depend(void* used_resource, void* user_resource)
{
    depend_impl({used_resource}, user_resource);
}

void garbage_collector::depend_many(argvec<void*> used_resource, void* user_resource)
{
    depend_impl(used_resource, user_resource);
}

void garbage_collector::depend(void* used_resource, VkSemaphore timeline, uint64_t value)
{
    depend_impl(used_resource, timeline, value);
}

void garbage_collector::remove(void* resource, std::function<void()>&& cleanup)
{
    std::unique_lock lk(mutex);
    resources[resource].cleanup = std::move(cleanup);
    check_delete(resource);
}

void garbage_collector::remove(VkSemaphore sem)
{
    std::unique_lock lk(mutex);
    semaphore_dependencies[sem].should_destroy = true;
}

// Checks all known semaphores, ensuring that they are ready for deletion.
void garbage_collector::collect()
{
    std::unique_lock lk(mutex);
    for(auto it = semaphore_dependencies.begin(); it != semaphore_dependencies.end();)
    {
        auto& triggers = it->second.triggers;
        uint64_t value = 0;
        vkGetSemaphoreCounterValue(dev->logical_device, it->first, &value);
        while(!triggers.empty() && triggers.top().value <= value)
        {
            if(triggers.top().callback)
                triggers.top().callback();

            if(triggers.top().dependent)
            {
                resources[triggers.top().dependent].dependency_count--;
                check_delete(triggers.top().dependent);
            }
            triggers.pop();
        }

        if(triggers.empty() && it->second.should_destroy)
        {
            vkDestroySemaphore(dev->logical_device, it->first, nullptr);
            it = semaphore_dependencies.erase(it);
        }
        else ++it;
    }
}

void garbage_collector::wait_collect()
{
    collect();
    if(resources.size() != 0 || semaphore_dependencies.size() != 0)
    {
        {
            std::unique_lock lk(mutex);
            vkDeviceWaitIdle(dev->logical_device);
        }

        collect();

#ifdef RAYBASE_GFX_GARBAGE_COLLECTOR_DEBUG
        for(auto& [res, dep]: resources)
        {
            if(dep.dependency_count == 0 && dep.dependents.size() != 0)
            {
                auto it = debug_infos.find(res);
                const char* message = "(unknown resource!)";
                if(it != debug_infos.end())
                    message = it->second.c_str();
                RB_LOG("Failed to free ", message);
            }
        }
#endif
    }
}

void garbage_collector::add_trigger(
    VkSemaphore timeline,
    uint64_t value,
    std::function<void()>&& callback
){
    std::unique_lock lk(mutex);
    semaphore_info& sem = semaphore_dependencies[timeline];
    sem.triggers.push({value, nullptr, std::move(callback)});
}

bool garbage_collector::trigger::operator<(const trigger& t) const
{
    return t.value < value;
}

void garbage_collector::depend_impl(argvec<void*> used_resource, void* user_resource)
{
    std::unique_lock lk(mutex);
    auto& dependents = resources[user_resource].dependents;
    dependents.insert(dependents.end(), used_resource.begin(), used_resource.end());
    for(void* res: used_resource)
        resources[res].dependency_count++;

    /*
    static size_t record = 0;
    size_t sz = dependents.size();
    if(sz > record)
    {
        printf("Record: %lu\n", record);
        record = sz;
    }
    */
}

void garbage_collector::depend_impl(void* used_resource, VkSemaphore timeline, uint64_t value)
{
    std::unique_lock lk(mutex);
    resources[used_resource].dependency_count++;
    semaphore_info& sem = semaphore_dependencies[timeline];
    sem.triggers.push({value, used_resource});
}

#ifdef RAYBASE_GFX_GARBAGE_COLLECTOR_DEBUG
//int64_t sync_trap_counter = 0;
#endif

void garbage_collector::check_delete(void* resource)
{
    auto it = resources.find(resource);
    if(it->second.dependency_count == 0 && it->second.cleanup)
    {
#ifdef RAYBASE_GFX_GARBAGE_COLLECTOR_DEBUG
        auto jt = debug_infos.find(resource);
        if(jt != debug_infos.end())
            RB_LOG("Deleting ", jt->second);
#endif
        it->second.cleanup();

#ifdef RAYBASE_GFX_GARBAGE_COLLECTOR_DEBUG
        /*
        sync_trap_counter--;
        if(sync_trap_counter == 0)
        {
            RB_LOG("running SYNC TRAP!");
            VkResult res = vkDeviceWaitIdle(dev->logical_device);
            RB_CHECK(res != VK_SUCCESS, "check_delete() failure ", res);
        }
        */
#endif

        for(void* dep: it->second.dependents)
        {
            resources[dep].dependency_count--;
            check_delete(dep);
        }
        resources.erase(it);
    }
}

}
