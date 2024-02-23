#ifndef RAYBASE_EXTRA_GLTF_HH
#define RAYBASE_EXTRA_GLTF_HH
#include "gfx/model.hh"
#include "core/skeleton.hh"
#include "gfx/light.hh"
#include "gfx/camera.hh"
#include "gfx/environment_map.hh"
#include "phys/shape.hh"
#include "phys/constraint.hh"
#include "core/filesystem.hh"
#include "core/ecs.hh"
#include <string>
#include <unordered_map>
#include <map>

namespace rb::extra
{

struct gltf_node
{
    std::string_view name;
    void* source;
};

}

namespace rb
{

template<>
class search_index<extra::gltf_node>
{
public:
    entity find(const std::string_view& name) const;

    void add_entity(entity id, const extra::gltf_node& data);
    void remove_entity(entity id, const extra::gltf_node& data);
    void update(scene& e);

private:
    std::unordered_map<std::string_view, entity> index;
};

}

namespace rb::extra
{

struct gltf_data_internal;

class gltf_data
{
public:
    struct options
    {
        // If load_extras is enabled, attempts to load associated files in the same
        // folder as the main file, with filenames of pattern
        // filename.extrafile.extension (.glb is stripped from the filename in that
        // pattern). If that file is not found or load_extras is false, the
        // environment_map and sh_grid objects will not have textures.
        bool load_extras = true;

        // Aggressive static marks everything as static, _unless_ they have
        // animations or dynamic colliders. Otherwise, only static colliders
        // get marked as static.
        bool aggressive_static = false;
    };

    // By default, everything is rendered. Use the 'foreach' function in order
    // to remove the rb::gfx::rendered component if you don't want this.
    gltf_data(gfx::device& dev, phys::context* pctx, const file& f, const options& opt);
    gltf_data(gltf_data&& other) noexcept;
    gltf_data(const gltf_data& other) = delete;
    ~gltf_data();

    gltf_data& operator=(gltf_data&& other) noexcept;

    // for resource_store
    static gltf_data load_resource(
        const file& f, gfx::device& dev, phys::context* pctx = nullptr,
        const options& opt = {true, false}
    );
    bool is_loaded() const;
    void wait() const;

    // Prints all entries in the object hierarchy. Use for debugging.
    void dump_objects() const;

    // Adds all entities to the ECS. You can optionally limit loading to a
    // single tree of nodes.
    std::vector<entity> add(scene& e, const std::string& root_name = "");

    // Removes all entities sourced from this file from the given ECS.
    void remove(scene& e);

    // Calls function for all entities sourced from this gltf file.
    void foreach(scene& e, const std::function<void(entity id)>& f);

    gfx::model get_model(const std::string& name) const;

    gfx::device& get_device() const;

private:
    static void load_resources(
        gfx::device& dev,
        phys::context* pctx,
        const file& f,
        gltf_data_internal* data,
        const options& opt
    );

    options opt;
    gfx::device* dev;
    mutable thread_pool::ticket loading_ticket;
    std::unique_ptr<gltf_data_internal> data;
    std::string source_path;
};

}

namespace rb
{
    template<>
    struct resource_loader<extra::gltf_data>: async_resource_loader<extra::gltf_data> {};
}

#endif
