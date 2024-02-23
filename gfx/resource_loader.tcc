#ifndef RAYBASE_GFX_RESOURCE_LOADER_TCC
#define RAYBASE_GFX_RESOURCE_LOADER_TCC
#include "resource_loader.hh"
#include "device.hh"
#include "context.hh"

namespace rb::gfx
{

template<typename T>
async_loadable_resource<T>::async_loadable_resource(device& dev)
: dev(&dev), loaded_cache(false), data(new impl_data)
{
    data->dev = &dev;
}

template<typename T>
async_loadable_resource<T>::~async_loadable_resource()
{
    if(data)
        wait();
}

template<typename T>
device& async_loadable_resource<T>::get_device() const
{
    return *dev;
}

template<typename T>
bool async_loadable_resource<T>::is_loaded() const
{
    if(loaded_cache) return true;

    for(const thread_pool::ticket& ticket: loading_tickets)
        if(!ticket.finished())
            return false;
    loading_tickets.clear();

    for(const event& e: data->loading_events)
        if(!e.finished(*dev))
            return false;
    data->loading_events.clear();

    loaded_cache = true;
    return true;
}

template<typename T>
void async_loadable_resource<T>::wait() const
{
    for(thread_pool::ticket& ticket: loading_tickets)
        ticket.wait();
    loading_tickets.clear();

    for(event& e: data->loading_events)
        e.wait(*dev);
    data->loading_events.clear();
    loaded_cache = true;
}

template<typename T>
const std::vector<thread_pool::ticket>& async_loadable_resource<T>::get_loading_tickets() const
{
    return loading_tickets;
}

template<typename T>
void async_loadable_resource<T>::add_loading_event(event e)
{
    loaded_cache = false;
    impl().loading_events.push_back(e);
}

template<typename T>
template<typename F, typename U>
void async_loadable_resource<T>::async_load(F&& f, const async_loadable_resource<U>& wait_for)
{
    async_load(std::forward<F>(f), wait_for.loading_tickets);
}

template<typename T>
template<typename F>
void async_loadable_resource<T>::async_load(F&& f, argvec<thread_pool::ticket> wait_for)
{
    loading_tickets.push_back(dev->ctx->get_thread_pool().add_task(f, 0, wait_for));
}

template<typename T>
typename async_loadable_resource<T>::impl_data&
async_loadable_resource<T>::impl(bool w) const
{
    if(w) wait();
    return *data;
}

}

#endif
