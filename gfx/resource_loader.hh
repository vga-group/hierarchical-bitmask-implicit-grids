#ifndef RAYBASE_GFX_RESOURCE_LOADER_HH
#define RAYBASE_GFX_RESOURCE_LOADER_HH
#include "core/resource_store.hh"
#include "core/thread_pool.hh"
#include "event.hh"
#include <mutex>

namespace rb::gfx
{

// General base class for graphics resources, that makes it a bit easier to
// write resources that get loaded in a thread pool behind the scenes.
template<typename T>
class async_loadable_resource
{
public:
    async_loadable_resource(device& dev);
    async_loadable_resource(async_loadable_resource&& other) noexcept = default;
    ~async_loadable_resource();

    async_loadable_resource& operator=(async_loadable_resource&& other) noexcept = default;

    device& get_device() const;

    // If you don't wait until the resource is loaded, everything should still
    // be well, but you may get some CPU-side waits when you try to access
    // certain parts of the resource.
    bool is_loaded() const;
    void wait() const;

    const std::vector<thread_pool::ticket>& get_loading_tickets() const;

    // If an external function somehow modifies the resource in a way that could
    // be construed as loading, you can add more loading events with this one.
    void add_loading_event(event e);

protected:
    template<typename F, typename U>
    void async_load(F&& f, const async_loadable_resource<U>& wait_for);
    template<typename F>
    void async_load(F&& f, argvec<thread_pool::ticket> wait_for = {});

    struct impl_data: T::impl_data
    {
        device* dev;
        mutable std::vector<event> loading_events;
    };
    impl_data& impl(bool wait = true) const;

    device* dev;
    mutable std::vector<thread_pool::ticket> loading_tickets;

private:
    mutable bool loaded_cache;
    std::unique_ptr<impl_data> data;
};

}

#include "resource_loader.tcc"

#endif

