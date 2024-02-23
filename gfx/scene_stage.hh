#ifndef RAYBASE_SCENE_STAGE_HH
#define RAYBASE_SCENE_STAGE_HH

#include "core/ecs.hh"
#include "core/bvh.hh"
#include "core/unique_index_table.hh"
#include "device.hh"
#include "render_stage.hh"
#include "timer.hh"
#include "gpu_buffer.hh"
#include "texture.hh"
#include "sampler.hh"
#include "primitive.hh"
#include "material.hh"
#include "camera.hh"
#include "decal.hh"
#include "environment_map.hh"
#include "compute_pipeline.hh"
#include "acceleration_structure.hh"
#include "radix_sort.hh"
#include <optional>

namespace rb::gfx
{

static constexpr uint32_t TLAS_DYNAMIC_MESH_MASK = 1<<0;
static constexpr uint32_t TLAS_STATIC_MESH_MASK = 1<<1;
static constexpr uint32_t TLAS_LIGHT_MASK = 1<<2;

struct rendered
{
    // Can be used to limit rendering a given object, which can be useful
    // for splitting stuff in separate passes in rasterization.
    uint32_t mask = 0xFFFFFFFF;
};

// Can be used to force a specific order for cameras regardless of entity ID.
// Useful in multi-view scenarios.
struct camera_order
{
    uint32_t order_index;
};

struct temporal_instance_data
{
    size_t update_hash;
    size_t prev_update_hash;
    uint32_t prev_base_instance_id;
    size_t prev_group_count;
    aabb bounding_box;
    mat4 prev_transform_matrix;
};

struct temporal_camera_data
{
    mat4 prev_view_projection_matrix;
    mat4 prev_inv_view_matrix;
    vec4 prev_proj_info;
};

struct temporal_light_data
{
    size_t update_hash;
    size_t prev_update_hash;
    uint32_t prev_index;
    aabb bounding_box;
    uint32_t frames_since_last_dynamic_visibility_change = 0;
    uint32_t frames_since_last_static_visibility_change = 0;
    bool shadow_outdated = true;
    bool static_shadow_outdated = true;
    flat_set<entity> dynamic_entities_in_range, static_entities_in_range;
};

class gpu_pipeline;
class clustering_stage;
class mesh;

// Manages scene data structures needed by other stages.
class scene_stage: public render_stage
{
public:
    struct options
    {
        uint32_t max_textures = 2048;
        uint32_t max_envmaps = 128;
        uint32_t max_primitives = 1024;

        // Keep this as low as you can. Must be a multiple of 128.
        // A hierarchical light clustering mode will be introduced at 1024, so
        // you'll probably want to avoid that. This hierarchy is good for up to
        // 16384, after which performance starts to degrade rapidly.
        // Acceleration structure memory usage is linear to the number of lights.
        uint32_t max_lights = 128;

        // Decals are also clustered, just like lights. They live in a separate
        // cluster, so parameters can be adjusted separately. The same caveats
        // apply.
        uint32_t max_decals = 128;

        uint32_t max_shadow_maps = 128;

        // Can increase performance by culling entries from render lists that
        // are outside of each camera frustum. But if you are CPU-bound and far
        // from being GPU-bound, you may just want to disable this.
        bool frustum_culling = true;

        // Can increase performance by reducing overdraw: coarsely sorts
        // render lists such that objects nearest to the camera are rendered
        // first.
        bool depth_sort = true;

        // Enables building and updating acceleration structures needed for ray
        // tracing.
        bool ray_tracing = true;

        // Enables building temporal ID mappings, needed by some few temporal
        // algorithms like ReSTIR. Not every temporal algorithm needs these.
        bool build_temporal_tables = true;

        // Defines the set of supported material features.
        uint32_t material_features =
            MATERIAL_FEATURE_ALBEDO |
            MATERIAL_FEATURE_METALLIC_ROUGHNESS |
            MATERIAL_FEATURE_EMISSION |
            MATERIAL_FEATURE_NORMAL_MAP |
            MATERIAL_FEATURE_TRANSMISSION |
            MATERIAL_FEATURE_ATTENUATION |
            MATERIAL_FEATURE_ENVMAP_SPECULAR |
            MATERIAL_FEATURE_ENVMAP_PARALLAX |
            MATERIAL_FEATURE_LIGHTMAP |
            MATERIAL_FEATURE_SH_PROBES;

        acceleration_structure_manager::options as_manager_opt;
    };

    scene_stage(device& dev, const options& opt);
    scene_stage(scene_stage&&) = delete;
    ~scene_stage();

    void set_scene(scene* s);
    scene* get_scene() const;

    const descriptor_set& get_descriptor_set() const;
    // These are used for translating indices of instances and lights between
    // frames. Contains mappings back and forth, with -1 marking invalid
    // mappings.
    const descriptor_set& get_temporal_tables_descriptor_set() const;
    void get_specialization_info(specialization_info& info) const;

    int32_t get_camera_index(entity id) const;
    int32_t get_envmap_index(entity id) const;

    uint32_t get_active_point_light_count() const;
    uint32_t get_active_directional_light_count() const;
    uint32_t get_active_tri_light_count() const;
    bool has_active_envmap() const;

    struct render_entry
    {
        entity id;
        uint32_t mask;
        size_t vertex_group_index;
        size_t instance_index;
        float depth;
    };
    const std::vector<render_entry>& get_render_list(
        entity cull_camera_id = INVALID_ENTITY
    ) const;
    const std::vector<entity>& get_active_cameras() const;

    VkAccelerationStructureKHR get_tlas_handle() const;
    bool has_temporal_tlas() const;

    bool has_material_feature(const uint32_t feature) const;

protected:
    void update_buffers(uint32_t frame_index) override;

private:
    friend class clustering_stage;

    void add_instances(
        void* data,
        size_t& index,
        argvec<frustum> camera_frusta,
        bool static_instances
    );

    bool update_object_buffers(uint32_t frame_index);
    bool update_light_buffers(uint32_t frame_index);
    bool update_decal_buffers(uint32_t frame_index);
    bool update_envmap_buffers(uint32_t frame_index);
    void update_bindings(uint32_t frame_index, bool need_descriptor_set_update);
    void update_temporal_tables(uint32_t frame_index);

    static int32_t add_texture(scene_stage* self, material::sampler_tex tex, sampler* default_sampler);
    void run_animation_update(VkCommandBuffer cmd);
    void run_tri_light_update(VkCommandBuffer cmd);

    options opt;
    scene* current_scene;
    clustering_stage* cluster_provider;
    timer stage_timer;

    bvh<entity> envmap_bvh;

    size_t instance_count;
    size_t instance_prev_count;
    size_t point_light_count;
    size_t point_light_prev_count;
    size_t directional_light_count;
    size_t tri_light_count;

    size_t shadow_map_count;
    size_t decal_count;
    size_t camera_count;
    size_t morph_target_weight_count;
    size_t matrix_joint_count;
    size_t dualquat_joint_count;

    gpu_buffer instances;
    gpu_buffer unsorted_point_lights;
    gpu_buffer unsorted_decals;
    gpu_buffer decal_metadata;
    gpu_buffer envmap_metadata;
    gpu_buffer directional_lights;
    gpu_buffer tri_lights;
    gpu_buffer shadow_map_info;
    gpu_buffer cameras;
    gpu_buffer morph_target_weights;
    gpu_buffer skeletal_joints;
    gpu_buffer scene_params;

    std::vector<uint32_t> instance_prev_map;
    std::vector<uint32_t> unsorted_point_light_prev_map;
    uint32_t instance_forward_offset;
    uint32_t instance_backward_offset;
    uint32_t point_light_forward_offset;
    uint32_t point_light_backward_offset;
    uint32_t sorted_point_light_forward_offset;
    uint32_t sorted_point_light_backward_offset;
    vkres<VkBuffer> prev_point_lights;
    gpu_buffer temporal_tables;

    vec3 light_bounds[2];
    vec3 decal_bounds[2];

    unique_index_table<const primitive*> primitive_indices;
    unique_index_table<material::sampler_tex> sampler_indices;
    unique_index_table<entity> envmap_indices;
    unique_index_table<mesh*> animated_mesh_indices;

    std::vector<VkImageView> textures;
    std::vector<VkSampler> samplers;
    std::vector<VkImageView> envmap_textures;
    std::vector<VkSampler> envmap_samplers;
    std::vector<VkImageView> shadow_textures;
    std::vector<VkSampler> shadow_test_samplers;
    std::vector<VkSampler> shadow_samplers;
    std::vector<entity> camera_indices;
    specialization_info specialization;

    // This always contains all instances, in order.
    // (i.e. generic_render_list[i].instance_index == i)
    std::vector<render_entry> generic_render_list;
    // These are culled by camera frustum.
    std::vector<std::vector<render_entry>> camera_render_list;

    // pos, normal and tangent always reside in the same buffer. The rest can
    // be from another set of buffers. They all have different offsets, though.
    std::vector<VkBuffer> vertex_pnt_buffers;
    std::vector<uint32_t> pos_offsets;
    std::vector<uint32_t> normal_offsets;
    std::vector<uint32_t> tangent_offsets;
    std::vector<uint32_t> prev_pos_offsets;
    std::vector<VkBuffer> vertex_uv_buffers;
    std::vector<uint32_t> texture_uv_offsets;
    std::vector<uint32_t> lightmap_uv_offsets;
    std::vector<VkBuffer> vertex_skeletal_buffers;
    std::vector<uint32_t> joints_offsets;
    std::vector<uint32_t> weights_offsets;
    std::vector<VkBuffer> index_buffers;

    texture blue_noise_texture;
    texture material_lut_texture;
    sampler blue_noise_sampler;
    sampler material_lut_sampler;
    sampler radiance_sampler;
    sampler default_texture_sampler;
    sampler default_decal_sampler;
    sampler shadow_test_sampler;
    sampler shadow_sampler;

    compute_pipeline animation_pipeline;
    compute_pipeline tri_light_pipeline;

    descriptor_set scene_data_set;
    descriptor_set temporal_tables_set;
    descriptor_set animation_set;
    push_descriptor_set tri_light_set;
    std::optional<acceleration_structure_manager> as_manager;
};

}

#endif
