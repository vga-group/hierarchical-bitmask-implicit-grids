#ifndef RAYBASE_RESOURCE_STORE_HH
#define RAYBASE_RESOURCE_STORE_HH
#include "filesystem.hh"
#include "types.hh"
#include "error.hh"
#include <unordered_map>
#include <functional>
#include <vector>
#include <memory>
#include <typeindex>
#include <atomic>
#include <optional>

namespace rb
{

// The default resource loader is stupid for now.
template<typename T>
class resource_loader
{
public:
    // Starts loading the resource
    template<typename... Args>
    void start_loading(const file& f, Args&&... args);

    // Should return nullptr when the resource has not finished loading yet.
    T* get() const;
    bool is_loaded() const;
    void wait();

private:
    std::optional<T> resource;
};

// A resource loader for graphics resources that also takes GPU availability
// into account. You type must define void wait() and bool is_loaded() const.
// Use it for your resource type like so:
// template<> struct resource_loader<mytype>: async_resource_loader<mytype> {};
template<typename T>
class async_resource_loader
{
public:
    // Starts loading the resource
    template<typename... Args>
    void start_loading(const file& f, Args&&... args);

    // Should return nullptr when the resource has not finished loading yet.
    T* get() const;
    bool is_loaded() const;
    void wait();

private:
    std::optional<T> resource;
};

// You should use the resource store to avoid unnecessary duplication of
// resources, and it can also be used to abstract the filesystem.
//
// All references are valid as long as the store is valid: you can use the
// nested resource_store constructor to have control over when resources are
// destroyed. For example, you would add a layer for a level in a game, and
// destroy that layer when exiting the level. Together with data_prefix, this
// lets you share certain resources across levels while others are
// level-specific.
//
// Every resource type should have a static member 'load_resource()', which
// returns an instance of the type and the first parameter is 'const file&'.
// The rest of the fields are free.
class resource_store
{
public:
    using load_callback = std::function<void()>;

    resource_store(
        const std::string& data_prefix,
        resource_store& parent
    );
    resource_store(
        const std::string& data_prefix,
        const std::vector<filesystem*>& data_sources
    );
    resource_store(resource_store&&) = delete;
    ~resource_store();

    // Starts loading the given resource. You can use the given resource_loader
    // to access it once it is available.
    template<typename T, typename... Args>
    resource_loader<T>& load(const std::string& filename, Args&&... args);

    // Starts loading the given resource and if necessary, waits until a handle
    // can be given. This does not mean that the resource is fully loaded; for
    // most types, calling any of the given resource's methods will cause it to
    // finish loading.
    template<typename T, typename... Args>
    T& get(const std::string& filename, Args&&... args);

    // Used for checking if some resource is still being asynchronously loaded.
    // Whenever you call get() for a new resource, this function may return true
    // for a while afterwards.
    bool is_loading() const;

    // Waits until all known resources are loaded.
    void wait();

    // You can use these two functions to implement a loading bar for loading
    // screens.
    size_t get_total_resource_count() const;
    size_t get_loaded_resource_count() const;

private:
    std::string get_path(const std::string& resource_name) const;
    file find_file(const std::string& resource_name) const;
    filesystem* find_filesystem(const std::string& path) const;

    resource_store* parent;
    std::vector<filesystem*> data_sources;

    struct basic_resource_pool
    {
        virtual ~basic_resource_pool() = default;
        virtual size_t get_total_resource_count() const = 0;
        virtual size_t get_loaded_resource_count() const = 0;
        virtual void wait() = 0;
    };

    template<typename T>
    struct resource_pool: public basic_resource_pool
    {
        size_t get_total_resource_count() const;
        size_t get_loaded_resource_count() const;
        void wait();

        std::unordered_map<std::string, resource_loader<T>> pool;
    };

    template<typename T>
    resource_pool<T>& get_resource_pool();

    std::unordered_map<
        std::type_index,
        std::unique_ptr<basic_resource_pool>
    > pools;

    std::string data_prefix;
};

}

#include "resource_store.tcc"

#endif
