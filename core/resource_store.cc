#include "resource_store.hh"
#include <stdexcept>
#include <thread>

namespace rb
{

resource_store::resource_store(
    const std::string& data_prefix,
    resource_store& parent
):  parent(&parent), data_sources(parent.data_sources), data_prefix(data_prefix)
{
}

resource_store::resource_store(
    const std::string& data_prefix,
    const std::vector<filesystem*>& data_sources
):  parent(nullptr), data_sources(data_sources), data_prefix(data_prefix)
{
}

resource_store::~resource_store()
{
}

bool resource_store::is_loading() const
{
    // TODO: Write a faster implementation if this becomes a problem.
    return get_total_resource_count() > get_loaded_resource_count();
}

void resource_store::wait()
{
    for(auto& pair: pools)
        pair.second->wait();
}

size_t resource_store::get_total_resource_count() const
{
    size_t sum = 0;
    for(const auto& pair: pools)
        sum += pair.second->get_total_resource_count();
    return sum;
}

size_t resource_store::get_loaded_resource_count() const
{
    size_t sum = 0;
    for(const auto& pair: pools)
        sum += pair.second->get_loaded_resource_count();
    return sum;
}

std::string resource_store::get_path(const std::string& resource_name) const
{
    if(data_prefix.empty())
        return resource_name;
    return data_prefix + "/" + resource_name;
}

file resource_store::find_file(
    const std::string& resource_name
) const
{
    std::string path = get_path(resource_name);

    for(filesystem* src: data_sources)
        if(src->exists(path))
            return src->get(path);

    if(parent) return parent->find_file(resource_name);
    throw std::runtime_error("Cannot find resource " + resource_name);
}

filesystem* resource_store::find_filesystem(const std::string& path) const
{
    for(filesystem* src: data_sources)
        if(src->exists(path)) return src;
    return nullptr;
}

}
