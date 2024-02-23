#ifndef RAYBASE_RESOURCE_STORE_TCC
#define RAYBASE_RESOURCE_STORE_TCC
#include "error.hh"
#include "resource_store.hh"

namespace rb
{

template<typename T>
template<typename... Args>
void resource_loader<T>::start_loading(
    const file& f, Args&&... args
){
    resource.emplace(T::load_resource(f, args...));
}

// Should return nullptr when the resource has not finished loading yet.
template<typename T>
T* resource_loader<T>::get() const
{
    return resource ? const_cast<T*>(&resource.value()) : nullptr;
}

template<typename T>
bool resource_loader<T>::is_loaded() const
{
    return resource;
}

template<typename T>
void resource_loader<T>::wait()
{
}

template<typename T>
template<typename... Args>
void async_resource_loader<T>::start_loading(const file& f, Args&&... args)
{
    resource.emplace(T::load_resource(f, args...));
}

template<typename T>
T* async_resource_loader<T>::get() const
{
    return resource ? const_cast<T*>(&resource.value()) : nullptr;
}

template<typename T>
bool async_resource_loader<T>::is_loaded() const
{
    return resource && resource->is_loaded();
}

template<typename T>
void async_resource_loader<T>::wait()
{
    resource->wait();
}

template<typename T, typename... Args>
resource_loader<T>& resource_store::load(const std::string& filename, Args&&... args)
{
    std::string path(get_path(filename));
    auto& pool = get_resource_pool<T>();
    auto it = pool.pool.find(path);
    if(it != pool.pool.end()) return it->second;
    else
    {
        filesystem* fs = find_filesystem(path);
        if(!fs)
        {
            if(parent)
                return parent->load<T>(filename, std::forward<Args>(args)...);
            else RB_PANIC("No such file: ", path);
        }

        it = pool.pool.emplace(path, resource_loader<T>()).first;

        it->second.start_loading(
            fs->get(path),
            std::forward<Args>(args)...
        );
        return it->second;
    }
}

template<typename T, typename... Args>
T& resource_store::get(const std::string& filename, Args&&... args)
{
    resource_loader<T>& ld = load<T>(filename, std::forward<Args>(args)...);
    T* t = ld.get();
    if(t) return *t;
    ld.wait();
    return *ld.get();
}

template<typename T>
size_t resource_store::resource_pool<T>::get_total_resource_count() const
{
    return pool.size();
}

template<typename T>
size_t resource_store::resource_pool<T>::get_loaded_resource_count() const
{
    size_t loaded = 0;
    for(const auto& [name, res]: pool)
        if(res.is_loaded())
            loaded++;
    return loaded;
}

template<typename T>
void resource_store::resource_pool<T>::wait()
{
    for(auto& [name, res]: pool)
        if(!res.is_loaded())
            res.wait();
}

template<typename T>
resource_store::resource_pool<T>& resource_store::get_resource_pool()
{
    std::type_index ti(typeid(T));
    auto it = pools.find(ti);
    if(it == pools.end())
        it = pools.emplace(ti, std::make_unique<resource_pool<T>>()).first;
    return *static_cast<resource_pool<T>*>(it->second.get());
}

}

#endif

