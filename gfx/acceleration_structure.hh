#ifndef RAYBASE_GFX_ACCELERATION_STRUCTURE_HH
#define RAYBASE_GFX_ACCELERATION_STRUCTURE_HH
#include "context.hh"
#include "resource_loader.hh"
#include "core/argvec.hh"
#include "core/transformable.hh"
#include "gpu_buffer.hh"
#include "descriptor_set.hh"
#include "compute_pipeline.hh"
#include "vkres.hh"
#include <optional>

namespace rb::gfx
{

class primitive;
class mesh;

// You can add this as a component to instances. If it's not defined, the
// default values are used.
struct ray_tracing_params
{
    bool face_culling = false;
    bool force_opaque = false;
};

// While it's only used from the mesh, it is more convenient to have it as a
// separate class to get the async loading working nicely. Users of raybase
// need not instantiate this class on their own, it's just used internally.
class bottom_level_acceleration_structure:
    public async_loadable_resource<bottom_level_acceleration_structure>
{
public:
    struct entry
    {
        const primitive* prim = nullptr;
        std::optional<mat4> transform;
    };
    bottom_level_acceleration_structure(
        device& dev,
        argvec<entry> primitives,
        bool dynamic = true
    );
    // Creates a BLAS that is compatible with the given other BLAS. Its current
    // contents are also copied over.
    bottom_level_acceleration_structure(
        bottom_level_acceleration_structure* compatible
    );
    // Creates a BLAS with a single AABB. This is useful for custom geometry.
    // It cannot be updated dynamically, because there is no reason to do so
    // (you can use transforms in the TLAS to achieve the same result.)
    bottom_level_acceleration_structure(
        device& dev,
        aabb box = {vec3(-1), vec3(1)}
    );
    bottom_level_acceleration_structure(const bottom_level_acceleration_structure& other) = delete;
    bottom_level_acceleration_structure(bottom_level_acceleration_structure&& other) noexcept = default;

    VkAccelerationStructureKHR get_handle() const;
    VkDeviceAddress get_device_address() const;

    void refresh(
        argvec<entry> primitives,
        VkCommandBuffer cmd,
        uint32_t frame_index,
        bool rebuild = false
    );
    // Equivalent to refresh() if rebuild = true. Uses 'src' to speed up update
    // of itself.
    void refresh_from(
        argvec<entry> primitives,
        VkCommandBuffer cmd,
        uint32_t frame_index,
        bottom_level_acceleration_structure& src,
        bool rebuild = false
    );
    size_t get_refreshes_since_last_rebuild() const;

    struct impl_data
    {
        bool dynamic;
        size_t refreshes_since_last_rebuild;
        size_t geometry_count;

        size_t acceleration_structure_size = 0;
        size_t scratch_size = 0;
        vkres<VkAccelerationStructureKHR> as;
        vkres<VkBuffer> as_buffer;
        VkDeviceAddress as_address;
        vkres<VkBuffer> as_scratch;
        VkDeviceAddress as_scratch_address;
        vkres<VkBuffer> aabb_buffer;

        std::optional<gpu_buffer> transform_buffer;
        VkDeviceAddress transform_address;
    };

private:
    using super_impl_data = async_loadable_resource<bottom_level_acceleration_structure>::impl_data;
    static void refresh_internal(
        super_impl_data& data,
        argvec<entry> primitives,
        VkCommandBuffer cmd,
        uint32_t frame_index,
        VkAccelerationStructureKHR src,
        bool rebuild
    );
    static void refresh_internal(
        super_impl_data& data,
        aabb box,
        VkCommandBuffer cmd,
        bool rebuild
    );
    static void copy_init_internal(
        super_impl_data& data,
        super_impl_data& other_data
    );
    static void refresh_internal_core(
        super_impl_data& data,
        argvec<VkAccelerationStructureGeometryKHR> geometries,
        argvec<VkAccelerationStructureBuildRangeInfoKHR> ranges,
        argvec<uint32_t> max_primitive_counts,
        VkCommandBuffer cmd,
        uint32_t frame_index,
        VkAccelerationStructureKHR src,
        bool rebuild
    );
};

class top_level_acceleration_structure
{
public:
    top_level_acceleration_structure(device& dev, size_t capacity);

    VkAccelerationStructureKHR get_handle() const;

    gpu_buffer& get_instances_buffer();
    void refresh(
        scene& s,
        size_t instance_count,
        VkCommandBuffer cmd,
        uint32_t frame_index
    );
    void copy(
        top_level_acceleration_structure& other,
        VkCommandBuffer cmd
    );

private:
    device* dev;

    size_t geometry_count;
    size_t geometry_capacity;

    vkres<VkAccelerationStructureKHR> tlas;
    vkres<VkBuffer> tlas_buffer;
    VkDeviceAddress tlas_buffer_address;
    vkres<VkBuffer> tlas_scratch;
    VkDeviceAddress tlas_scratch_address;

    std::optional<gpu_buffer> tlas_instances;
    VkDeviceAddress tlas_instances_address;
};

// Maintains BLASes and TLAS(es) according to the selected strategy.
class acceleration_structure_manager
{
public:
    enum blas_strategy
    {
        // Per-mesh is a safe assumption, but can be suboptimal if there's a
        // lot of separate, truly static models.
        PER_MESH = 0,

        // Merges all static objects into one BLAS (dynamic objects are still
        // per-model). Generally faster, but you have to be careful to avoid
        // moving, adding or removing static objects as that triggers a costly
        // rebuild.
        MERGE_STATIC
    };

    struct options
    {
        blas_strategy strategy = MERGE_STATIC;

        // You can optionally keep the previous acceleration structures around
        // for a frame. This is needed for unbiased MIS with temporal reuse in
        // ReSTIR, for example.
        bool keep_previous = false;

        // If enabled, unused per-model BLASes are kept around until
        // clear_unused() is called. Otherwise, they're removed ASAP.
        bool lazy_removal = false;

        // If enabled, TLAS instances will allow having culled backfaces. This
        // harms performance.
        bool force_allow_face_culling = false;
    };

    acceleration_structure_manager(
        device& dev,
        descriptor_set& scene_data_set,
        size_t capacity,
        const options& opt
    );

    void update(
        VkCommandBuffer cmd,
        uint32_t frame_index,
        scene& s,
        descriptor_set& scene_data_set,
        size_t point_light_count,
        // Should be scene_stage::render_entry, but forward declaration issues
        // prevent me from calling it what it is.
        void* instance_entries,
        size_t instance_count,
        argvec<mesh*> animated_meshes
    );
    void clear_unused();
    // Makes prev := current and removes previous acceleration structures
    // immediately. Only meaningful if opt.keep_previous == true.
    void clear_previous();

    VkAccelerationStructureKHR get_tlas() const;
    // May also return the current TLAS - don't rely on these being unique.
    VkAccelerationStructureKHR get_prev_tlas() const;

private:
    void select_blas_groups(
        scene& s,
        uint32_t frame_index,
        void* instance_entries,
        size_t instance_count
    );
    void build_blas_groups(
        scene& s,
        void* instance_entries,
        size_t instance_count
    );
    void animate_blas_groups(
        scene& s,
        argvec<mesh*> animated_meshes,
        VkCommandBuffer cmd,
        uint32_t frame_index
    );

    struct instance_group
    {
        uint64_t blas_id = 0;
        size_t instance_count = 0;
        bool static_transform = false;
        bool static_mesh = false;
    };

    device* dev;
    options opt;
    compute_pipeline point_light_tlas_instance_pipeline;
    descriptor_set tlas_instance_data_set;

    bool has_previous_data;
    top_level_acceleration_structure tlas;
    // This one only gets copied into. BLASes alternate, but TLASes don't
    // because that makes it easier to bind them around in descriptor sets.
    std::optional<top_level_acceleration_structure> prev_tlas;

    bottom_level_acceleration_structure point_light_blas;

    std::vector<instance_group> instance_groups;
    std::vector<VkAccelerationStructureKHR> prev_blas_handles;
    struct blas_data
    {
        blas_data(
            size_t last_use_frame,
            device& dev,
            argvec<bottom_level_acceleration_structure::entry> primitives,
            bool dynamic,
            bool create_alternate
        );

        size_t last_use_frame;
        bool current_blas_is_alternate = false;
        bottom_level_acceleration_structure primary_blas;
        std::optional<bottom_level_acceleration_structure> alternate_blas;
    };
    std::unordered_map<uint64_t, blas_data> blas;
};

}

#endif
