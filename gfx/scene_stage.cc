#include "scene_stage.hh"
#include "core/stack_allocator.hh"
#include "vulkan_helpers.hh"
#include "clustering_stage.hh"
#include "model.hh"
#include "camera.hh"
#include "light.hh"
#include "gpu_pipeline.hh"
#include "path_tracer.hh"
#include "animation.comp.h"
#include "tri_light_extract.comp.h"
#include "material_lut.hh"

#define INSTANCES_BUFFER_ALIGNMENT 16
#define MAX_ACTIVE_MORPH_TARGETS_PER_MESH 8
#define CLUSTER_AXIS_COUNT 3

namespace
{
using namespace rb;
using namespace rb::gfx;

struct gpu_material
{
    // FACTORS
    // xyz = color, w = alpha (8-bit unorm)
    uint32_t color;
    // x = metallic, y = roughness, z = transmission, w = translucency (8-bit unorm)
    uint32_t metallic_roughness_transmission_translucency;
    // xyzw = emission (RGBE, 8-bit unorm)
    uint32_t emission;
    // xyz = transmission attenuation, w = (ior-1)/2 (8-bit unorm)
    uint32_t attenuation_ior;
    // x = 1 if double sided, y = alpha cutoff, z = unused, w = unused (8-bit unorm)
    uint32_t double_sided_cutoff;
    // x = clearcoat factor, y = clearcoat roughness, z = anisotropy, w = anisotropy rotation (8-bit unorm)
    uint32_t clearcoat_factor_roughness_anisotropy;
    // xyz = sheen color, w = sheen roughness (8-bit unorm)
    uint32_t sheen_color_roughness;
    // x, y, z = subsurface radius per color, w = subsurface strength
    uint32_t subsurface;

    // TEXTURES (0xFFFF means unset!)
    // x = color + alpha, y = metallic+roughness (uint16_t)
    uint32_t color_metallic_roughness_textures;
    // x = normal, y = emission (uint16_t)
    uint32_t normal_emission_textures;
    // x = clearcoat factor + clearcoat roughness, y = clearcoat normal (uint16_t)
    uint32_t clearcoat_textures;
    // x = transmission + translucency, y = sheen (uint16_t)
    uint32_t transmission_sheen_textures;
};

constexpr uint32_t INSTANCE_FLAG_RECEIVES_DECALS = 1<<0;
constexpr uint32_t INSTANCE_FLAG_HAS_TRI_LIGHTS = 1<<1;

struct gpu_instance
{
    pmat4 model_to_world;
    pmat4 normal_to_world;
    pmat4 prev_model_to_world;
    gpu_material material;
    // x = cubemap index, y = cubemap parallax type, z = lightmap index, w = SH grid index
    pivec4 environment;
    uint32_t buffer_index;
    uint32_t attribute_mask;
    uint32_t flags;
    uint32_t triangle_count;
};

struct gpu_decal
{
    pmat4 world_to_obb;
    gpu_material material;
};

struct gpu_decal_metadata
{
    float pos_x, pos_y, pos_z;
    uint32_t order;
};

struct gpu_camera
{
    pmat4 view_proj;
    pmat4 inv_view;
    pmat4 inv_view_proj;
    pmat4 prev_view_proj;
    pmat4 prev_inv_view;
    pvec4 proj_info;
    pvec4 prev_proj_info;
    float focal_distance;
    float aperture_radius;
    float aperture_angle;
    float pad[1];
    // TODO Jitter for TAA!
};

struct gpu_point_light
{
    float pos_x;
    float pos_y;
    float pos_z;
    uint32_t color; // RGBE
    uint32_t radius_and_cutoff_radius; // Packed halfs
    uint32_t cutoff_angle_and_directional_falloff_exponent; // Packed halfs
    uint32_t direction; // Two packed halfs, octahedral encoding
    uint32_t shadow_map_index_and_spot_radius; // uint16_t and half
};

struct gpu_directional_light
{
    uint32_t color; // RGBE
    uint32_t direction; // Two packed halfs, octahedral encoding
    float solid_angle;
    int shadow_map_index;
};

struct gpu_shadow_map
{
    // Takes a world space point to the light's space.
    pmat4 world_to_shadow;
    // If directional shadow map, number of additional cascades. Otherwise, 1 if
    // perspective, 6 if omni.
    int cascade_count;
    // packed halfs
    uint32_t pcf_radius;
    // If directional shadow map, contains packed halfs of cascade offsets.
    // Otherwise, four first entries contain projection info as floats
    // (use intBitsToFloat to read)
    uint32_t cascades[6];
};

static_assert(sizeof(gpu_shadow_map) == 96, "gpu_shadow_map size is not 96!");

struct gpu_tri_light
{
    pvec4 corners[3]; // Corner positions in RGB, floatBitsToUint & unpackHalf2x16 UV coordinate in A
    uint32_t emission_factor; // RGBE
    uint32_t emission_tex; // Could be compressed to 16 bits in the future.
    // These map the triangle light to its original instance.
    uint32_t instance_id;
    uint32_t triangle_id;
};

static_assert(sizeof(gpu_tri_light) == 64, "gpu_tri_light size is not 64!");

struct gpu_envmap_metadata
{
    // Takes a world space point to the envmap's space, needed for parallax
    // envmaps.
    pmat4 world_to_envmap;
    // parallax type and texture index are in gpu_instance instead! That allows
    // skipping reading these gpu_envmap structs entirely!
};

struct gpu_sh_probe
{
    float coef_data[3*9];
};

struct animation_push_constant_buffer
{
    uint32_t morph_target_count;
    uint32_t morph_target_weight_offset;
    uint32_t vertex_count;

    int32_t base_buffer_index;

    int32_t morph_target_ptn_indices[MAX_ACTIVE_MORPH_TARGETS_PER_MESH];
    int32_t morph_target_has_positions;
    int32_t morph_target_has_normals;
    int32_t morph_target_has_tangents;

    int32_t dst_position_index;
    int32_t dst_normal_index;
    int32_t dst_tangent_index;
    int32_t dst_prev_position_index;

    uint32_t joint_count;
    int32_t joint_matrices_index;
    int32_t joint_dualquats_index;
};

static_assert(
    sizeof(animation_push_constant_buffer) <= 128,
    "push constant buffer must not exceed minimum maximum of 128."
);

struct tri_light_push_constant_buffer
{
    uint32_t triangle_offset;
    uint32_t triangle_count;
    uint32_t instance_id;
};

struct scene_param_buffer
{
    puvec4 light_cluster_size;
    pvec4 light_cluster_inv_slice;
    pvec4 light_cluster_offset;
    puvec4 light_cluster_axis_offsets;
    puvec4 light_hierarchy_axis_offsets;
    puvec4 decal_cluster_size;
    pvec4 decal_cluster_inv_slice;
    pvec4 decal_cluster_offset;
    puvec4 decal_cluster_axis_offsets;
    puvec4 decal_hierarchy_axis_offsets;
    pvec4 envmap_orientation;
    int32_t envmap_index;
    uint32_t envmap_face_size_x;
    uint32_t envmap_face_size_y;
    uint32_t point_light_count;
    uint32_t directional_light_count;
    uint32_t tri_light_count;
    uint32_t decal_count;
    uint32_t instance_count;
};

uint32_t tex_pack(int a, int b)
{
    RB_CHECK(
        a >= 0xFFFF || b >= 0xFFFF,
        "Using over 65535 textures simultaneously is not supported"
    );
    if(a < 0) a = 0xFFFF;
    if(b < 0) b = 0xFFFF;
    return uint32_t(a) | (uint32_t(b)<<16);
}

bool keep_morph_target(float weight)
{
    return weight > 1e-6;
}

vec2 align_cascade(vec2 offset, vec2 area, float scale, uvec2 resolution)
{
    vec2 cascade_step_size = (area * scale) / vec2(resolution);
    return round(offset / cascade_step_size) * cascade_step_size;
}

void update_light_visibility_info_part(
    scene& s,
    aabb bounding_box,
    flat_set<entity>& entities_in_range,
    bool& changed,
    bool require_static,
    bool require_dynamic
){
    for(
        entity* it = entities_in_range.begin();
        it < entities_in_range.end();
    ){
        bool remove = false;

        temporal_instance_data* sm_status = s.get<temporal_instance_data>(*it);
        transformable* t = s.get<transformable>(*it);
        model* m = s.get<model>(*it);
        if(!sm_status || !t || !m || !m->m)
        { // Entity stopped existing
            remove = true;
        }
        else if(
            (require_static && !t->is_static()) ||
            (require_dynamic && t->is_static())
        ){ // Static-ness has changed.
            remove = true;
        }
        else if(sm_status->update_hash != sm_status->prev_update_hash)
        { // Entity is just changed
            changed = true;
            // If it is now out of range, just remove it.
            if(!aabb_overlap(bounding_box, sm_status->bounding_box))
                remove = true;
        }

        if(remove)
        {
            it = entities_in_range.erase(*it);
            changed = true;
        }
        else ++it;
    }
}

void update_light_visibility_info(
    scene& s,
    temporal_light_data& status
){
    if(status.update_hash != status.prev_update_hash)
    { // Light source itself has moved, so all known entities are now outdated.
        status.shadow_outdated = true;
        status.dynamic_entities_in_range.clear();
        status.static_shadow_outdated = true;
        status.static_entities_in_range.clear();
        status.frames_since_last_dynamic_visibility_change = 0;
        status.frames_since_last_static_visibility_change = 0;
    }
    else
    { // Otherwise, check which ones we should remove
        bool shadow_outdated = false;
        bool static_shadow_outdated = false;
        update_light_visibility_info_part(
            s, status.bounding_box, status.dynamic_entities_in_range,
            shadow_outdated, false, true
        );
        update_light_visibility_info_part(
            s, status.bounding_box, status.static_entities_in_range,
            static_shadow_outdated, true, false
        );
        if(static_shadow_outdated)
        {
            shadow_outdated = true;
            status.frames_since_last_static_visibility_change = 0;
        }
        status.shadow_outdated |= shadow_outdated;
        status.static_shadow_outdated |= static_shadow_outdated;
        if(shadow_outdated)
            status.frames_since_last_dynamic_visibility_change = 0;
    }
}

template<typename F, typename T>
gpu_material material_to_gpu_material(
    const material& mat,
    sampler* default_sampler,
    const scene_stage::options& opt,
    F&& add_texture,
    T* self
) {
    gpu_material gm;
    gm.color = packUnorm4x8(mat.color);
    gm.metallic_roughness_transmission_translucency = packUnorm4x8(vec4(
        mat.metallic,
        mat.roughness,
        mat.transmission,
        mat.translucency
    ));
    gm.emission = rgb_to_rgbe(mat.emission);
    gm.attenuation_ior = packUnorm4x8(vec4(
        mat.volume_attenuation,
        (mat.ior-1.0f)*0.5f
    ));
    gm.double_sided_cutoff = packUnorm4x8(vec4(
        mat.double_sided ? 1.0f: 0.0f,
        mat.alpha_cutoff < 0 ? 1.0f : mat.alpha_cutoff,
        0.0f,
        0.0f
    ));
    gm.clearcoat_factor_roughness_anisotropy = packUnorm4x8(vec4(
        mat.clearcoat,
        mat.clearcoat_roughness,
        mat.anisotropy,
        mat.anisotropy_rotation
    ));
    gm.sheen_color_roughness = packUnorm4x8(vec4(
        mat.sheen_color,
        mat.sheen_roughness
    ));
    gm.subsurface = 0;

    gm.color_metallic_roughness_textures = tex_pack(
        (opt.material_features & MATERIAL_FEATURE_ALBEDO) ? add_texture(self, mat.color_texture, default_sampler) : -1,
        (opt.material_features & MATERIAL_FEATURE_METALLIC_ROUGHNESS) ? add_texture(self, mat.metallic_roughness_texture, default_sampler) : -1
    );
    gm.normal_emission_textures = tex_pack(
        (opt.material_features & MATERIAL_FEATURE_NORMAL_MAP) ? add_texture(self, mat.normal_texture, default_sampler) : -1,
        (opt.material_features & MATERIAL_FEATURE_EMISSION) ? add_texture(self, mat.emission_texture, default_sampler) : -1
    );
    gm.clearcoat_textures = tex_pack(
        (opt.material_features & MATERIAL_FEATURE_CLEARCOAT) ? add_texture(self, mat.clearcoat_texture, default_sampler) : -1,
        (opt.material_features & MATERIAL_FEATURE_CLEARCOAT) ? add_texture(self, mat.clearcoat_normal_texture, default_sampler) : -1
    );
    gm.transmission_sheen_textures = tex_pack(
        (opt.material_features & MATERIAL_FEATURE_TRANSMISSION) ? add_texture(self, mat.transmission_translucency_texture, default_sampler) : -1,
        (opt.material_features & MATERIAL_FEATURE_SHEEN) ? add_texture(self, mat.sheen_texture, default_sampler) : -1
    );

    return gm;
};

}

namespace rb::gfx
{

scene_stage::scene_stage(device& dev, const options& opt)
:   render_stage(dev),
    opt(opt),
    current_scene(nullptr),
    cluster_provider(nullptr),
    stage_timer(dev, "scene data refresh"),
    instance_count(0),
    instance_prev_count(0),
    point_light_count(0),
    point_light_prev_count(0),
    directional_light_count(0),
    tri_light_count(0),
    shadow_map_count(0),
    decal_count(0),
    camera_count(0),
    instances(dev, sizeof(gpu_instance), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    unsorted_point_lights(dev, sizeof(gpu_point_light) * opt.max_lights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
    unsorted_decals(dev, sizeof(gpu_decal) * opt.max_decals, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    decal_metadata(dev, sizeof(gpu_decal_metadata) * opt.max_decals, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    envmap_metadata(dev, sizeof(gpu_envmap_metadata) * opt.max_envmaps, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    directional_lights(dev, sizeof(gpu_directional_light), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    tri_lights(dev, sizeof(gpu_tri_light), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    shadow_map_info(dev, sizeof(gpu_shadow_map), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    cameras(dev, sizeof(gpu_camera), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    morph_target_weights(dev, sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    skeletal_joints(dev, sizeof(pmat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    scene_params(dev, sizeof(scene_param_buffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
    temporal_tables(dev, sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    blue_noise_texture(dev,
        {
            uvec3(256, 256, 1), VK_FORMAT_R32G32B32A32_SFLOAT, 1,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        }, 256*256*sizeof(vec4), generate_blue_noise<vec4>(uvec2(256,256)).data()
    ),
    material_lut_texture(dev,
        {
            uvec3(32, 32, 1), VK_FORMAT_R8G8B8A8_UNORM, 1,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        }, sizeof(material_lut), material_lut
    ),
    blue_noise_sampler(dev, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, 0.0f),
    material_lut_sampler(dev, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, 0.0f),
    radiance_sampler(dev, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 1),
    default_texture_sampler(dev),
    default_decal_sampler(
        dev, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    ),
    shadow_test_sampler(
        dev, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0, 0.0f, 0.0f, true
    ),
    shadow_sampler(
        dev, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0, 0.0f, 0.0f, false
    ),
    animation_pipeline(dev),
    tri_light_pipeline(dev),
    scene_data_set(dev),
    temporal_tables_set(dev),
    animation_set(dev),
    tri_light_set(dev)
{
    RB_CHECK(opt.max_lights % 128u != 0, "max_lights must be divisible by 128.");
    RB_CHECK(opt.max_decals % 128u != 0, "max_decals must be divisible by 128.");
    textures.reserve(opt.max_textures);
    samplers.reserve(opt.max_textures);
    envmap_textures.reserve(opt.max_envmaps);
    envmap_samplers.reserve(opt.max_envmaps);
    shadow_textures.reserve(opt.max_shadow_maps);
    shadow_test_samplers.reserve(opt.max_shadow_maps);
    shadow_samplers.reserve(opt.max_shadow_maps);

    vertex_pnt_buffers.reserve(opt.max_primitives);
    pos_offsets.reserve(opt.max_primitives);
    normal_offsets.reserve(opt.max_primitives);
    tangent_offsets.reserve(opt.max_primitives);
    prev_pos_offsets.reserve(opt.max_primitives);
    vertex_uv_buffers.reserve(opt.max_primitives);
    texture_uv_offsets.reserve(opt.max_primitives);
    lightmap_uv_offsets.reserve(opt.max_primitives);
    index_buffers.reserve(opt.max_primitives);

    vertex_skeletal_buffers.reserve(opt.max_primitives);
    joints_offsets.reserve(opt.max_primitives);
    weights_offsets.reserve(opt.max_primitives);

    using namespace std::literals;
    shader_data animation_shader(animation_comp_shader_binary);

    animation_set.add(animation_shader);
    animation_set.set_binding_params("vertex_position", opt.max_primitives, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
    animation_set.set_binding_params("vertex_normal", opt.max_primitives, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
    animation_set.set_binding_params("vertex_tangent", opt.max_primitives, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
    animation_set.set_binding_params("vertex_prev_position", opt.max_primitives, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
    animation_set.set_binding_params("vertex_joints", opt.max_primitives, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
    animation_set.set_binding_params("vertex_weights", opt.max_primitives, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);

    animation_pipeline.init(
        animation_shader,
        sizeof(animation_push_constant_buffer),
        animation_set.get_layout()
    );

    scene_data_set.add("textures",
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, opt.max_textures, VK_SHADER_STAGE_ALL, nullptr},
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    );
    scene_data_set.add("cube_textures",
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, opt.max_envmaps * 2, VK_SHADER_STAGE_ALL, nullptr},
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    );
    scene_data_set.add("instances", {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    scene_data_set.add("point_lights", {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    scene_data_set.add("directional_lights", {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    scene_data_set.add("tri_lights", {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    scene_data_set.add("decals", {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    scene_data_set.add("cameras", {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    scene_data_set.add("vertex_position",
        {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, opt.max_primitives, VK_SHADER_STAGE_ALL, nullptr},
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    );
    scene_data_set.add("vertex_prev_position",
        {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, opt.max_primitives, VK_SHADER_STAGE_ALL, nullptr},
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    );
    scene_data_set.add("vertex_normal",
        {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, opt.max_primitives, VK_SHADER_STAGE_ALL, nullptr},
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    );
    scene_data_set.add("vertex_tangent",
        {11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, opt.max_primitives, VK_SHADER_STAGE_ALL, nullptr},
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    );
    scene_data_set.add("vertex_texture_uv",
        {12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, opt.max_primitives, VK_SHADER_STAGE_ALL, nullptr},
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    );
    scene_data_set.add("vertex_lightmap_uv",
        {13, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, opt.max_primitives, VK_SHADER_STAGE_ALL, nullptr},
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    );
    scene_data_set.add("indices",
        {14, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, opt.max_primitives, VK_SHADER_STAGE_ALL, nullptr},
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    );
    scene_data_set.add("light_cluster_slices", {15, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    scene_data_set.add("decal_cluster_slices", {16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    scene_data_set.add("scene_params", {17, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    scene_data_set.add("material_lut", {18, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, nullptr});
    scene_data_set.add("shadow_map_info", {19, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    scene_data_set.add("shadow_test_textures",
        {20, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, opt.max_shadow_maps, VK_SHADER_STAGE_ALL, nullptr},
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    );
    scene_data_set.add("shadow_textures",
        {21, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, opt.max_shadow_maps, VK_SHADER_STAGE_ALL, nullptr},
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    );
    scene_data_set.add("blue_noise_lut", {22, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, nullptr});
    scene_data_set.add("envmap_metadata", {23, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});

    if(opt.ray_tracing)
    {
        scene_data_set.add("scene_tlas", {27, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_ALL, nullptr});
        scene_data_set.add("scene_prev_tlas", {28, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_ALL, nullptr});

        // Point lights are also included in the TLAS!
        as_manager.emplace(dev, scene_data_set, opt.max_primitives + opt.max_lights, opt.as_manager_opt);
    }

    temporal_tables_set.add("instance_map_forward", {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    temporal_tables_set.add("instance_map_backward", {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    temporal_tables_set.add("point_light_map_forward", {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    temporal_tables_set.add("point_light_map_backward", {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    temporal_tables_set.add("prev_point_lights", {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});

    shader_data tri_light_shader(tri_light_extract_comp_shader_binary);
    tri_light_set.add(tri_light_shader);
    tri_light_pipeline.init(
        tri_light_shader,
        sizeof(tri_light_push_constant_buffer),
        {tri_light_set.get_layout(), scene_data_set.get_layout()}
    );

    if(opt.build_temporal_tables)
    {
        prev_point_lights = create_gpu_buffer(
            dev, sizeof(gpu_point_light) * opt.max_lights,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT
        );
    }
}

scene_stage::~scene_stage()
{
}

void scene_stage::set_scene(scene* s)
{
    if(current_scene == s)
        return;

    current_scene = s;
}

scene* scene_stage::get_scene() const
{
    return current_scene;
}

const descriptor_set& scene_stage::get_descriptor_set() const
{
    return scene_data_set;
}

const descriptor_set& scene_stage::get_temporal_tables_descriptor_set() const
{
    RB_CHECK(
        !opt.build_temporal_tables,
        "Temporal ID mapping tables are not enabled in the scene, yet someone "
        "is trying to use them (likely some temporal rendering algorithm?) "
        "Set scene_stage::options::build_temporal_tables = true!"
    );
    return temporal_tables_set;
}

void scene_stage::get_specialization_info(specialization_info& info) const
{
    info[0] = opt.material_features;
}

int32_t scene_stage::get_camera_index(entity id) const
{
    auto it = std::find(
        camera_indices.begin(),
        camera_indices.end(), id
    );
    if(it == camera_indices.end() || *it != id)
        return -1;
    return it - camera_indices.begin();
}

int32_t scene_stage::get_envmap_index(entity id) const
{
    return envmap_indices.get(id);
}

uint32_t scene_stage::get_active_point_light_count() const
{
    return point_light_count;
}

uint32_t scene_stage::get_active_directional_light_count() const
{
    return directional_light_count;
}

uint32_t scene_stage::get_active_tri_light_count() const
{
    return tri_light_count;
}

bool scene_stage::has_active_envmap() const
{
    entity envmap_id = find_sky_envmap(*current_scene);
    return envmap_id != INVALID_ENTITY;
}

const std::vector<scene_stage::render_entry>& scene_stage::get_render_list(
    entity cull_camera_id
) const {
    (void)cull_camera_id;
    int32_t i = get_camera_index(cull_camera_id);
    if(i < 0 || i >= camera_render_list.size())
        return generic_render_list;
    return camera_render_list[i];
}

const std::vector<entity>& scene_stage::get_active_cameras() const
{
    return camera_indices;
}

VkAccelerationStructureKHR scene_stage::get_tlas_handle() const
{
    if(as_manager.has_value()) return as_manager->get_tlas();
    else return VK_NULL_HANDLE;
}

bool scene_stage::has_temporal_tlas() const
{
    return opt.as_manager_opt.keep_previous;
}

bool scene_stage::has_material_feature(const uint32_t feature) const
{
    return opt.material_features & feature;
}

void scene_stage::update_buffers(uint32_t frame_index)
{
    // Full reset of all containers at the start.
    bool need_descriptor_set_update = false;

    envmap_indices.clear();
    primitive_indices.clear();
    sampler_indices.clear();
    camera_indices.clear();
    generic_render_list.clear();
    animated_mesh_indices.clear();
    for(auto& list: camera_render_list)
        list.clear();

    // If we don't have a scene, just do an empty pass and skip the rest.
    if(!current_scene)
    {
        clear_commands();
        VkCommandBuffer cmd = compute_commands(true);
        stage_timer.start(cmd, frame_index);
        stage_timer.stop(cmd, frame_index);
        use_compute_commands(cmd, frame_index);
        return;
    }

    // The order is important here, don't reorder these unless you absolutely
    // know what you're doing!
    need_descriptor_set_update |= update_envmap_buffers(frame_index);
    need_descriptor_set_update |= update_object_buffers(frame_index);
    need_descriptor_set_update |= update_light_buffers(frame_index);
    need_descriptor_set_update |= update_decal_buffers(frame_index);
    update_bindings(frame_index, need_descriptor_set_update);
    update_temporal_tables(frame_index);

    // Record update command buffers
    clear_commands();
    VkCommandBuffer cmd = compute_commands(true);
    stage_timer.start(cmd, frame_index);

    if(opt.build_temporal_tables && point_light_prev_count != 0)
    { // Before we update the lights, we save them from the previous frame.
        VkBuffer point_lights_buf;
        if(cluster_provider && cluster_provider->sorted_point_lights)
            point_lights_buf = (VkBuffer)cluster_provider->sorted_point_lights;
        else point_lights_buf = (VkBuffer)unsorted_point_lights;

        VkBufferCopy region = {0, 0, point_light_prev_count * sizeof(gpu_point_light)};
        vkCmdCopyBuffer(cmd, point_lights_buf, prev_point_lights, 1, &region);
        dev->gc.depend_many({point_lights_buf, prev_point_lights}, cmd);
    }

    upload(
        cmd,
        {
            &instances,
            &unsorted_point_lights,
            &directional_lights,
            &unsorted_decals,
            &decal_metadata,
            &envmap_metadata,
            &cameras,
            &morph_target_weights,
            &skeletal_joints,
            &scene_params,
            &temporal_tables,
            &shadow_map_info
        },
        frame_index
    );
    run_animation_update(cmd);
    if(opt.ray_tracing)
    {
        run_tri_light_update(cmd);
        as_manager->update(
            cmd, frame_index, *current_scene,
            scene_data_set, point_light_count,
            generic_render_list.data(),
            generic_render_list.size(),
            argvec<mesh*>(animated_mesh_indices.begin(), animated_mesh_indices.size())
        );
    }
    stage_timer.stop(cmd, frame_index);
    use_compute_commands(cmd, frame_index);
}

void scene_stage::add_instances(
    void* vdata,
    size_t& i,
    argvec<frustum> camera_frusta,
    bool static_instances
){
    auto camera_local_frusta = global_stack_allocator.allocate<frustum>(camera_count);
    auto camera_local_depths = global_stack_allocator.allocate<float>(camera_count);

    bool tri_lights_enabled = opt.ray_tracing;
    gpu_instance* data = (gpu_instance*)vdata;
    instance_prev_map.resize(instance_count);
    current_scene->foreach([&](entity id, rendered& r, transformable& t, model& m, temporal_instance_data* td, disable_decals* dd) {
        bool static_instance = t.is_static() && m.m && !m.m->is_animated();
        if(static_instance != static_instances)
            return;

        size_t update_hash = 1;
        hash_combine(update_hash, t.update_cached_transform());
        if(m.m)
        {
            hash_combine(update_hash, m.m);
            hash_combine(update_hash, m.m->get_animation_state_hash());
        }

        mat4 model_mat = t.get_global_transform();
        mat4 trans_mat = transpose(model_mat);
        mat4 inv = inverse(trans_mat);
        mat4 prev_mat = model_mat;
        vec3 pos = get_matrix_translation(model_mat);
        size_t group_count = m.m ? m.m->group_count() : 0;
        bool group_count_changed = false;
        uint32_t prev_base_instance_id = 0;

        // Handle previous transform matrix data
        if(td)
        {
            prev_mat = td->prev_transform_matrix;
            td->prev_update_hash = td->update_hash;
            td->update_hash = update_hash;
            group_count_changed = td->prev_group_count != group_count;
            td->prev_group_count = group_count;
            prev_base_instance_id = td->prev_base_instance_id;
            td->prev_base_instance_id = i;
            td->prev_transform_matrix = model_mat;
        }
        else
        {
            current_scene->attach(id, temporal_instance_data{update_hash, 0, (uint32_t)i, group_count, aabb{}, model_mat});
            td = current_scene->get<temporal_instance_data>(id);
            group_count_changed = true;
        }

        // Put the instance in a BVH.
        if(td->prev_update_hash != td->update_hash)
        {
            aabb bounding_box = {vec3(pos), vec3(pos)};
            if(m.m) m.m->get_bounding_box(bounding_box);
            td->bounding_box = aabb_from_obb(bounding_box, t.get_global_transform());
        }

        // Calculate local frusta if necessary
        if(opt.frustum_culling && m.m && m.m->has_any_bounding_boxes())
        {
            for(size_t j = 0; j < camera_frusta.size(); ++j)
            {
                camera_local_frusta[j] = camera_frusta[j];
                for(vec4& p: camera_local_frusta[j].planes)
                    p = trans_mat * p;
            }
        }

        // Pre-calculate approx depth for each camera (needed for depth
        // sorting - all primitives in the model share same depth)
        if(opt.depth_sort)
        {
            for(size_t j = 0; j < camera_frusta.size(); ++j)
            {
                camera_local_depths[j] =
                    dot(camera_frusta[j].planes[5], vec4(pos, 1.0f));
            }
        }

        const auto& mt_weights = m.m->get_morph_target_weights();

        // Create render list entries for all primitives
        size_t group_index = 0;
        for(size_t g = 0; g < group_count; ++g)
        {
            const vertex_group& group = (*m.m)[g];
            const material& mat = m.materials[g];
            gpu_instance& inst = data[i];
            inst.model_to_world = model_mat;
            inst.normal_to_world = inv;
            inst.prev_model_to_world = prev_mat;
            inst.material = material_to_gpu_material(mat, &default_texture_sampler, opt, add_texture, this);
            inst.environment = ivec4(-1);

            // Find suitable envmap
            const environment_map* best_envmap = nullptr;
            entity best_envmap_id = INVALID_ENTITY;
            if(mat.force_envmap != INVALID_ENTITY)
            {
                best_envmap_id = mat.force_envmap;
                best_envmap = current_scene->get<environment_map>(best_envmap_id);
            }
            else
            {
                float best_volume = FLT_MAX;
                envmap_bvh.query(pos, [&](entity id){
                    transformable* envmap_t = current_scene->get<transformable>(id);
                    environment_map* envmap = current_scene->get<environment_map>(id);
                    if(envmap->point_inside(envmap_t, pos))
                    {
                        vec3 scaling = envmap_t->get_global_scaling();
                        float volume = scaling.x * scaling.y * scaling.z;
                        // Find tightest usable envmap
                        if(volume < best_volume)
                        {
                            best_volume = volume;
                            best_envmap = envmap;
                            best_envmap_id = id;
                        }
                    }
                });
            }

            if(best_envmap)
            {
                inst.environment.x = envmap_indices.get(best_envmap_id);
                inst.environment.y = (int)best_envmap->parallax;
            }

            // Add an SH probe.
            inst.environment.w = -1;

            inst.buffer_index = primitive_indices.insert(group.get_primitive());
            inst.attribute_mask = group.get_primitive()->get_available_attributes();
            inst.flags = 0;
            if(!dd)
                inst.flags |= INSTANCE_FLAG_RECEIVES_DECALS;

            size_t tri_count = group.get_primitive()->get_index_count()/3;
            if(mat.potentially_emissive() && tri_lights_enabled)
            {
                inst.flags |= INSTANCE_FLAG_HAS_TRI_LIGHTS;
                tri_light_count += tri_count;
            }

            inst.triangle_count = tri_count;

            // Also insert source mesh and morph targets!
            primitive_indices.insert(group.source);
            for(size_t j = 0; j < group.morph_targets.size(); ++j)
            {
                if(keep_morph_target(mt_weights[j]))
                    primitive_indices.insert(group.morph_targets[j]);
            }

            instance_prev_map[i] = group_count_changed ? 0xFFFFFFFFu : prev_base_instance_id+g;
            render_entry entry = {id, r.mask, group_index++, i++, 0.0f};
            generic_render_list.push_back(entry);

            aabb bounding_box;
            bool cullable = opt.frustum_culling && group.get_primitive()->get_bounding_box(bounding_box);
            for(size_t j = 0; j < camera_render_list.size(); ++j)
            {
                if(!cullable || aabb_frustum_cull(
                    bounding_box,
                    camera_local_frusta[j]
                )){
                    entry.depth = camera_local_depths[j];
                    camera_render_list[j].push_back(entry);
                }
            }
        }
    });
}

bool scene_stage::update_object_buffers(uint32_t frame_index)
{
    bool need_descriptor_set_update = false;

    instance_prev_count = instance_count;

    instance_count = 0;
    camera_count = 0;
    morph_target_weight_count = 0;
    matrix_joint_count = 0;
    tri_light_count = 0;
    dualquat_joint_count = 0;

    current_scene->foreach([&](entity id, rendered&, transformable& t, model& m){
        if(!m.m) return;
        instance_count += m.m->group_count();
        if(m.m->is_animation_dirty() && animated_mesh_indices.check_insert(m.m) >= 0)
        {
            const auto& mt_weights = m.m->get_morph_target_weights();
            size_t active_count = std::count_if(
                mt_weights.begin(), mt_weights.end(), keep_morph_target
            );
            morph_target_weight_count += active_count;

            RB_CHECK(
                active_count > MAX_ACTIVE_MORPH_TARGETS_PER_MESH,
                "A mesh has ", active_count, " active morph targets, but "
                "the maximum is ", MAX_ACTIVE_MORPH_TARGETS_PER_MESH
            );

            if(skeleton* skel = m.m->get_skeleton())
            {
                skeleton::skinning_mode skin = skel->get_skinning_mode();
                if(skin == skeleton::LINEAR)
                    matrix_joint_count += skel->get_true_joint_count();
                else if(skin == skeleton::DUAL_QUATERNION)
                    dualquat_joint_count += skel->get_true_joint_count();
            }
        }
    });

    current_scene->foreach([&](entity id, rendered&, transformable& t, camera& c){
        camera_count++;
        camera_indices.push_back(id);
    });

    std::stable_sort(
        camera_indices.begin(),
        camera_indices.end(),
        [&](entity a, entity b){
            camera_order* a_order = current_scene->get<camera_order>(a);
            camera_order* b_order = current_scene->get<camera_order>(b);

            if(!a_order) return false;
            if(!b_order) return true;
            return a_order->order_index < b_order->order_index;
        }
    );

    RB_CHECK(camera_count == 0, "No camera in scene!");
    auto camera_frusta = global_stack_allocator.allocate<frustum>(camera_count);
    camera_render_list.resize(camera_count);
    need_descriptor_set_update |= cameras.resize(camera_count * sizeof(gpu_camera));
    cameras.update<gpu_camera>(frame_index, [&](gpu_camera* data) {
        for(size_t i = 0; i < camera_indices.size(); ++i)
        {
            entity id = camera_indices[i];
            transformable& t = *current_scene->get<transformable>(id);
            camera& c = *current_scene->get<camera>(id);
            temporal_camera_data* td = current_scene->get<temporal_camera_data>(id);

            mat4 inv_view = t.get_global_transform();
            mat4 view = inverse(inv_view);
            mat4 proj = c.get_projection();

            mat4 view_proj = proj * view;
            mat4 inv_view_proj = inverse(view_proj);
            mat4 prev_view_proj = view_proj;
            vec4 prev_proj_info = c.get_projection_info();
            mat4 prev_inv_view = inv_view;
            if(td)
            {
                prev_view_proj = td->prev_view_projection_matrix;
                prev_proj_info = td->prev_proj_info;
                prev_inv_view = td->prev_inv_view_matrix;

                td->prev_view_projection_matrix = view_proj;
                td->prev_inv_view_matrix = inv_view;
                td->prev_proj_info = c.get_projection_info();
            }
            else
            {
                current_scene->attach(id, temporal_camera_data{
                    view_proj,
                    prev_inv_view,
                    prev_proj_info
                });
            }

            camera_frusta[i] = c.get_global_frustum(t);
            vec3 focus_info = c.get_focus_info();
            data[i] = {
                view_proj,
                inv_view,
                inv_view_proj,
                prev_view_proj,
                prev_inv_view,
                c.get_projection_info(),
                prev_proj_info,
                focus_info.x, focus_info.y, focus_info.z
            };
        }
    });

    need_descriptor_set_update |= instances.resize(instance_count * sizeof(gpu_instance));
    instances.update<gpu_instance>(frame_index, [&](gpu_instance* data) {
        size_t i = 0;
        // Static and dynamic instances should be segmented separately so that
        // merged static acceleration structures are easier to do.
        add_instances(data, i, camera_frusta, true);
        add_instances(data, i, camera_frusta, false);
    });

    need_descriptor_set_update |= morph_target_weights.resize(morph_target_weight_count * sizeof(float));
    morph_target_weights.update<float>(frame_index, [&](float* data){
        for(mesh* m: animated_mesh_indices)
        {
            const auto& weights = m->get_morph_target_weights();
            for(float w: weights)
            {
                if(keep_morph_target(w))
                {
                    *data = w;
                    data++;
                }
            }
        }
    });

    need_descriptor_set_update |= skeletal_joints.resize(
        matrix_joint_count * sizeof(mat4) +
        dualquat_joint_count * sizeof(dualquat)
    );
    skeletal_joints.update<float>(frame_index, [&](float* data){
        float* mat_ptr = data;
        float* dquat_ptr = data + matrix_joint_count * sizeof(mat4) / sizeof(float);
        for(mesh* m: animated_mesh_indices)
        {
            if(skeleton* skel = m->get_skeleton())
            {
                skeleton::skinning_mode skin = skel->get_skinning_mode();
                // TODO: Figure out how to avoid referring to some root
                // transformable in the joint transform refresh
                void* joint_data = skel->refresh_joint_transforms();
                if(skin == skeleton::LINEAR)
                {
                    size_t data_size = skel->get_true_joint_count() * sizeof(mat4);
                    memcpy(mat_ptr, joint_data, data_size);
                    mat_ptr += data_size / sizeof(float);
                }
                else if(skin == skeleton::DUAL_QUATERNION)
                {
                    size_t data_size = skel->get_true_joint_count() * sizeof(dualquat);
                    memcpy(dquat_ptr, joint_data, data_size);
                    dquat_ptr += data_size / sizeof(float);
                }
            }
        }
    });

    // Sort render lists if requested
    if(opt.depth_sort)
    {
        for(size_t j = 0; j < camera_render_list.size(); ++j)
        {
            // TODO: Radix sort
            std::sort(
                camera_render_list[j].begin(),
                camera_render_list[j].end(),
                [](const render_entry& a, const render_entry& b)
                {
                    return a.depth < b.depth;
                }
            );
        }
    }

    return need_descriptor_set_update;
}

bool scene_stage::update_light_buffers(uint32_t frame_index)
{
    bool need_descriptor_set_update = false;

    point_light_prev_count = point_light_count;

    point_light_count = 0;
    directional_light_count = 0;
    shadow_map_count = 0;

    current_scene->foreach([&](
        entity id,
        rendered&,
        transformable& t,
        point_light* pl,
        spotlight* sl,
        directional_light* dl
    ){
        if(pl) point_light_count++;
        if(sl) point_light_count++;
        if(dl) directional_light_count++;
    });
    shadow_textures.resize(shadow_map_count);
    shadow_test_samplers.resize(shadow_map_count, shadow_test_sampler.get());
    shadow_samplers.resize(shadow_map_count, shadow_sampler.get());

    RB_CHECK(
        point_light_count > opt.max_lights,
        "Too many lights in scene: ", point_light_count,
        ". Bump up scene_stage::options::max_lights!"
    );
    unsorted_point_lights.resize(point_light_count * sizeof(gpu_point_light));

    light_bounds[0] = vec3(FLT_MAX);
    light_bounds[1] = vec3(-FLT_MAX);

    unsorted_point_light_prev_map.resize(point_light_count);
    unsorted_point_lights.update<gpu_point_light>(frame_index, [&](gpu_point_light* data){
        size_t i = 0;
        current_scene->foreach([&](
            entity id,
            rendered&,
            transformable& t,
            point_light& l,
            temporal_light_data* td
        ) {
            vec3 pos = t.get_global_position();
            float cutoff = l.get_cutoff_radius();
            int shadow_map_index16 = 0xFFFF;
            data[i] = {
                pos.x, pos.y, pos.z, rgb_to_rgbe(l.color),
                packHalf2x16(vec2(l.radius, cutoff)),
                0, 0,
                (uint32_t)shadow_map_index16
            };
            light_bounds[0] = min(light_bounds[0], pos - cutoff);
            light_bounds[1] = max(light_bounds[1], pos + cutoff);

            size_t update_hash = 1;
            hash_combine(update_hash, t.update_cached_transform());
            hash_combine(update_hash, cutoff);

            if(!td)
            { // Initial update
                current_scene->attach(id, temporal_light_data{update_hash, 0, (uint32_t)i});
                unsorted_point_light_prev_map[i] = 0xFFFFFFFFu;
                td = current_scene->get<temporal_light_data>(id);
            }
            else
            {
                td->prev_update_hash = td->update_hash;
                unsorted_point_light_prev_map[i] = td->prev_index;
                td->prev_index = i;
                td->update_hash = update_hash;
            }

            if(td->prev_update_hash != td->update_hash)
            {
                vec3 center = t.get_global_position();
                td->bounding_box = {center-cutoff, center+cutoff};
            }
            td->frames_since_last_dynamic_visibility_change++;
            td->frames_since_last_static_visibility_change++;
            update_light_visibility_info(*current_scene, *td);

            i++;
        });
        current_scene->foreach([&](
            entity id,
            rendered&,
            transformable& t,
            spotlight& l,
            temporal_light_data* td
        ) {
            vec3 pos = t.get_global_position();
            float cutoff = l.get_cutoff_radius();
            int shadow_map_index16 = 0xFFFF;
            float spot_radius = l.cutoff_angle <= 89 ?  cutoff * tan(glm::radians(l.cutoff_angle)) : -1;
            data[i++] = {
                pos.x, pos.y, pos.z, rgb_to_rgbe(l.color),
                packHalf2x16(vec2(l.radius, cutoff)),
                packHalf2x16(
                    vec2(
                        1.0 - cos(radians(l.cutoff_angle)),
                        l.falloff_exponent
                    )
                ),
                packSnorm2x16(octahedral_encode(t.get_global_direction())),
                (uint32_t)shadow_map_index16 | (uint32_t(glm::detail::toFloat16(spot_radius)) << 16)
            };
            light_bounds[0] = min(light_bounds[0], pos - cutoff);
            light_bounds[1] = max(light_bounds[1], pos + cutoff);

            size_t update_hash = 1;
            hash_combine(update_hash, t.update_cached_transform());
            hash_combine(update_hash, cutoff);
            hash_combine(update_hash, l.cutoff_angle);

            if(!td)
            { // Initial update
                current_scene->attach(id, temporal_light_data{update_hash, 0});
                td = current_scene->get<temporal_light_data>(id);
            }
            else
            {
                td->prev_update_hash = td->update_hash;
                td->update_hash = update_hash;
            }

            if(td->prev_update_hash != td->update_hash)
            {
                vec3 center = t.get_global_position();
                vec3 dir = t.get_global_direction();
                vec3 e = sqrt(1.0f - dir * dir);
                vec3 pe = center + dir * cutoff;
                td->bounding_box = {
                    max(min(center, pe - e * spot_radius), center-cutoff),
                    min(max(center, pe + e * spot_radius), center+cutoff)
                };
            }
            td->frames_since_last_dynamic_visibility_change++;
            td->frames_since_last_static_visibility_change++;
            update_light_visibility_info(*current_scene, *td);
        });
    });

    need_descriptor_set_update |= directional_lights.resize(directional_light_count * sizeof(gpu_directional_light));
    directional_lights.update<gpu_directional_light>(frame_index, [&](gpu_directional_light* data){
        size_t i = 0;
        current_scene->foreach([&](entity id, rendered&, transformable& t, directional_light& l) {
            float solid_angle = l.angular_radius == 0 ? 0.0 : 1.0 - cos(radians((double)l.angular_radius));
            data[i++] = {
                rgb_to_rgbe(l.color),
                packSnorm2x16(octahedral_encode(t.get_global_direction())),
                solid_angle,
                -1
            };
        });
    });

    need_descriptor_set_update |= tri_lights.resize(tri_light_count * sizeof(gpu_tri_light));

    RB_CHECK(
        shadow_map_count > opt.max_shadow_maps,
        "Too many shadow maps in scene: ", shadow_map_count,
        ". Bump up scene_stage::options::max_shadow_maps!"
    );

    return need_descriptor_set_update;
}

bool scene_stage::update_decal_buffers(uint32_t frame_index)
{
    decal_count = 0;
    current_scene->foreach([&](entity id, rendered&, transformable& t, decal& dc){
        decal_count++;
    });

    RB_CHECK(
        decal_count > opt.max_decals,
        "Too many decals in scene: ", decal_count,
        ". Bump up scene_stage::options::max_decals!"
    );
    unsorted_decals.resize(decal_count * sizeof(gpu_decal));

    decal_bounds[0] = vec3(FLT_MAX);
    decal_bounds[1] = vec3(-FLT_MAX);

    unsorted_decals.update<gpu_decal>(frame_index, [&](gpu_decal* data){
        decal_metadata.update<gpu_decal_metadata>(frame_index, [&](gpu_decal_metadata* metadata){
            size_t i = 0;
            current_scene->foreach([&](entity id, rendered&, transformable& t, decal& d) {
                gpu_decal_metadata& gmd = metadata[i];
                gpu_decal& gd = data[i++];
                mat4 transform = t.get_global_transform();
                gd.world_to_obb = transpose(affineInverse(transform));
                gd.material = material_to_gpu_material(d.mat, &default_decal_sampler, opt, add_texture, this);

                vec3 pos, scale;
                quat orientation;
                decompose_matrix(transform, pos, scale, orientation);
                float cutoff = length(scale);

                gmd.pos_x = pos.x;
                gmd.pos_y = pos.y;
                gmd.pos_z = pos.z;
                gmd.order = d.order;

                // We use spherical bounds here, because it's faster and doesn't matter much.
                decal_bounds[0] = min(decal_bounds[0], pos - cutoff);
                decal_bounds[1] = max(decal_bounds[1], pos + cutoff);
            });
        });
    });
    return false;
}

bool scene_stage::update_envmap_buffers(uint32_t frame_index)
{
    envmap_bvh.clear();
    current_scene->foreach([&](
        entity id, rendered&, transformable& t, environment_map& em
    ){
        if(em.cubemap)
        {
            envmap_indices.insert(id);
            envmap_bvh.add(em.get_aabb(t), id);
        }
    });
    envmap_bvh.build(bvh_heuristic::EQUAL_COUNT);

    current_scene->foreach([&](entity id, rendered& r, transformable& t, model& m) {
        size_t group_count = m.m ? m.m->group_count() : 0;
        for(size_t g = 0; g < group_count; ++g)
        {
            const material& mat = m.materials[g];
            if(mat.force_envmap != INVALID_ENTITY)
                envmap_indices.insert(mat.force_envmap);
        }
    });

    RB_CHECK(
        envmap_indices.size() > opt.max_envmaps,
        "Too many envmaps in scene: ", envmap_indices.size(),
        ". Bump up scene_stage::options::max_envmaps!"
    );
    envmap_metadata.update<gpu_envmap_metadata>(frame_index, [&](gpu_envmap_metadata* metadata){
        for(size_t i = 0; i < envmap_indices.size(); ++i)
        {
            const transformable* envmap_transform = current_scene->get<transformable>(envmap_indices[i]);
            metadata[i].world_to_envmap = inverse(envmap_transform->get_global_transform());
        }
    });
    return false;
}

void scene_stage::update_bindings(uint32_t frame_index, bool need_descriptor_set_update)
{
    // Fill pos, normal, tangent and prev_pos bindings!
    if(vertex_pnt_buffers.size() != (uint32_t)primitive_indices.size())
    {
        need_descriptor_set_update = true;
        vertex_pnt_buffers.resize(primitive_indices.size());
        pos_offsets.resize(primitive_indices.size());
        normal_offsets.resize(primitive_indices.size());
        tangent_offsets.resize(primitive_indices.size());
        prev_pos_offsets.resize(primitive_indices.size());
        vertex_uv_buffers.resize(primitive_indices.size());
        texture_uv_offsets.resize(primitive_indices.size());
        lightmap_uv_offsets.resize(primitive_indices.size());
        vertex_skeletal_buffers.resize(primitive_indices.size());
        joints_offsets.resize(primitive_indices.size());
        weights_offsets.resize(primitive_indices.size());
        index_buffers.resize(primitive_indices.size());
    }

#define DS_UPDATE(dst, src) \
    { \
        auto stmp = src; \
        /*if(!need_descriptor_set_update && dst != stmp) printf("Tripped at " #dst "\n"); */ \
        need_descriptor_set_update |= (dst != stmp); \
        dst = stmp; \
    }

    for(size_t i = 0; i < primitive_indices.size(); ++i)
    {
        const primitive* prim = primitive_indices[i];
        VkBuffer index_buffer = prim->get_index_buffer();
        DS_UPDATE(index_buffers[i], index_buffer);
        DS_UPDATE(vertex_pnt_buffers[i], prim->get_vertex_buffer(primitive::POSITION));
        DS_UPDATE(pos_offsets[i], prim->get_attribute_offset(primitive::POSITION));
        DS_UPDATE(normal_offsets[i], prim->get_attribute_offset(primitive::NORMAL));
        DS_UPDATE(tangent_offsets[i], prim->get_attribute_offset(primitive::TANGENT));
        DS_UPDATE(prev_pos_offsets[i], prim->get_attribute_offset(primitive::PREV_POSITION));

        bool texture_uv = prim->has_attribute(primitive::TEXTURE_UV);
        bool lightmap_uv = prim->has_attribute(primitive::LIGHTMAP_UV);
        DS_UPDATE(
            vertex_uv_buffers[i],
            texture_uv || lightmap_uv ?
                prim->get_vertex_buffer(primitive::TEXTURE_UV) : VK_NULL_HANDLE
        );
        DS_UPDATE(texture_uv_offsets[i], texture_uv ? prim->get_attribute_offset(primitive::TEXTURE_UV) : 0);
        DS_UPDATE(lightmap_uv_offsets[i], lightmap_uv ? prim->get_attribute_offset(primitive::LIGHTMAP_UV) : 0);

        if(prim->has_attribute(primitive::JOINTS|primitive::WEIGHTS))
        {
            DS_UPDATE(vertex_skeletal_buffers[i], prim->get_vertex_buffer(primitive::JOINTS));
            DS_UPDATE(joints_offsets[i], prim->get_attribute_offset(primitive::JOINTS));
            DS_UPDATE(weights_offsets[i], prim->get_attribute_offset(primitive::WEIGHTS));
        }
        else
        {
            DS_UPDATE(vertex_skeletal_buffers[i], VK_NULL_HANDLE);
            DS_UPDATE(joints_offsets[i], 0);
            DS_UPDATE(weights_offsets[i], 0);
        }
    }

    // Update texture binding arrays
    RB_CHECK(
        sampler_indices.size() > opt.max_textures,
        "Too many textures in scene: ", sampler_indices.size(),
        ". Bump up scene_stage::options::max_textures!"
    );
    RB_CHECK(
        primitive_indices.size() > opt.max_primitives,
        "Too many primitives in scene: ", primitive_indices.size(),
        ". Bump up scene_stage::options::max_primitives!"
    );

    if(textures.size() != sampler_indices.size())
    {
        need_descriptor_set_update = true;
        textures.resize(sampler_indices.size());
        samplers.resize(sampler_indices.size());
    }

    for(size_t i = 0; i < sampler_indices.size(); ++i)
    {
        DS_UPDATE(textures[i], sampler_indices[i].second->get_image_view());
        DS_UPDATE(samplers[i], sampler_indices[i].first->get());
    }

    if(envmap_textures.size() != envmap_indices.size())
    {
        need_descriptor_set_update = true;
        envmap_textures.resize(envmap_indices.size());
        envmap_samplers.resize(envmap_indices.size());
    }

    for(size_t i = 0; i < envmap_indices.size(); ++i)
    {
        const environment_map* envmap = current_scene->get<environment_map>(envmap_indices[i]);
        DS_UPDATE(envmap_textures[i], envmap->cubemap->get_image_view());
        DS_UPDATE(envmap_samplers[i], radiance_sampler.get());
    }

    // Update scene metadata
    scene_param_buffer sp;
    if(cluster_provider)
    {
        for(uint32_t i = 0; i < CLUSTER_AXIS_COUNT; ++i)
        {
            sp.light_cluster_size[i] = cluster_provider->opt.light_cluster_resolution;
            sp.decal_cluster_size[i] = cluster_provider->opt.decal_cluster_resolution;
            vec2 bounds = vec2(light_bounds[0][i], light_bounds[1][i]);
            sp.light_cluster_inv_slice[i] = cluster_provider->opt.light_cluster_resolution / (bounds.y - bounds.x);
            sp.light_cluster_offset[i] = -bounds.x * sp.light_cluster_inv_slice[i];

            bounds = vec2(decal_bounds[0][i], decal_bounds[1][i]);
            sp.decal_cluster_inv_slice[i] = cluster_provider->opt.decal_cluster_resolution / (bounds.y - bounds.x);
            sp.decal_cluster_offset[i] = -bounds.x * sp.decal_cluster_inv_slice[i];
        }
        cluster_provider->get_light_cluster_axis_buffer_offsets(&sp.light_cluster_axis_offsets[0]);
        sp.light_cluster_axis_offsets /= sizeof(uvec4);
        sp.light_hierarchy_axis_offsets /= sizeof(uvec4);
        cluster_provider->get_decal_cluster_axis_buffer_offsets(&sp.decal_cluster_axis_offsets[0]);
        sp.decal_cluster_axis_offsets /= sizeof(uvec4);
        sp.decal_hierarchy_axis_offsets /= sizeof(uvec4);
    }
    else
    {
        sp.light_cluster_size = puvec4(0);
        sp.decal_cluster_size = puvec4(0);
    }
    sp.point_light_count = point_light_count;
    sp.directional_light_count = directional_light_count;
    sp.tri_light_count = tri_light_count;
    sp.decal_count = decal_count;
    sp.instance_count = instance_count;

    sp.envmap_orientation = vec4(0,0,0,1);
    sp.envmap_index = -1;
    sp.envmap_face_size_x = 0;
    sp.envmap_face_size_y = 0;

    entity envmap_id = find_sky_envmap(*current_scene);
    if(envmap_id != INVALID_ENTITY)
    {
        transformable* t = current_scene->get<transformable>(envmap_id);
        quat orientation = t ? inverse(t->get_global_orientation()) : quat(1,0,0,0);
        sp.envmap_orientation = vec4(
            orientation.x, orientation.y, orientation.z, orientation.w
        );
        sp.envmap_index = get_envmap_index(envmap_id);

        environment_map* em = current_scene->get<environment_map>(envmap_id);
        uvec2 size = em->cubemap->get_size();
        sp.envmap_face_size_x = size.x;
        sp.envmap_face_size_y = size.y;
    }

    scene_params.update(frame_index, sp);

    // Update descriptor set
    if(need_descriptor_set_update)
    {
        scene_data_set.reset(1);
        scene_data_set.set_image(0, "textures", textures, samplers);
        scene_data_set.set_image(0, "cube_textures", envmap_textures, envmap_samplers);
        scene_data_set.set_buffer(0, "instances", (VkBuffer)instances);
        // We don't sort lights if the light hierarchy isn't in use, so we just
        // use the unsorted ones then.
        if(cluster_provider && cluster_provider->sorted_point_lights)
            scene_data_set.set_buffer(0, "point_lights", (VkBuffer)cluster_provider->sorted_point_lights);
        else
            scene_data_set.set_buffer(0, "point_lights", (VkBuffer)unsorted_point_lights);
        if(cluster_provider && cluster_provider->sorted_decals)
            scene_data_set.set_buffer(0, "decals", (VkBuffer)cluster_provider->sorted_decals);
        else
            scene_data_set.set_buffer(0, "decals", (VkBuffer)unsorted_decals);
        scene_data_set.set_buffer(0, "directional_lights", (VkBuffer)directional_lights);
        scene_data_set.set_buffer(0, "tri_lights", (VkBuffer)tri_lights);
        scene_data_set.set_buffer(0, "cameras", (VkBuffer)cameras);
        scene_data_set.set_buffer(0, "vertex_position", vertex_pnt_buffers, pos_offsets);
        scene_data_set.set_buffer(0, "vertex_prev_position", vertex_pnt_buffers, prev_pos_offsets);
        scene_data_set.set_buffer(0, "vertex_normal", vertex_pnt_buffers, normal_offsets);
        scene_data_set.set_buffer(0, "vertex_tangent", vertex_pnt_buffers, tangent_offsets);
        scene_data_set.set_buffer(0, "vertex_texture_uv", vertex_uv_buffers, texture_uv_offsets);
        scene_data_set.set_buffer(0, "vertex_lightmap_uv", vertex_uv_buffers, lightmap_uv_offsets);
        scene_data_set.set_buffer(0, "indices", index_buffers);
        scene_data_set.set_buffer(0, "scene_params", (VkBuffer)scene_params);
        if(cluster_provider)
        {
            scene_data_set.set_buffer(0, "light_cluster_slices", (VkBuffer)cluster_provider->light_cluster_slices);
            scene_data_set.set_buffer(0, "decal_cluster_slices", (VkBuffer)cluster_provider->decal_cluster_slices);
        }
        else
        {
            // Just stick some random buffer in there so that validation layers
            // don't complain. These should never be read if there is no cluster
            // provider.
            scene_data_set.set_buffer(0, "light_cluster_slices", (VkBuffer)instances);
            scene_data_set.set_buffer(0, "decal_cluster_slices", (VkBuffer)instances);
        }
        scene_data_set.set_image(0, "material_lut", material_lut_texture.get_image_view(), material_lut_sampler.get());
        scene_data_set.set_buffer(0, "shadow_map_info", (VkBuffer)shadow_map_info);
        scene_data_set.set_image(0, "shadow_test_textures", shadow_textures, shadow_test_samplers);
        scene_data_set.set_image(0, "shadow_textures", shadow_textures, shadow_samplers);
        scene_data_set.set_image(0, "blue_noise_lut", blue_noise_texture.get_image_view(), blue_noise_sampler.get());
        scene_data_set.set_buffer(0, "envmap_metadata", (VkBuffer)envmap_metadata);

        if(opt.ray_tracing)
        {
            scene_data_set.set_acceleration_structure(0, "scene_tlas", as_manager->get_tlas());
            scene_data_set.set_acceleration_structure(0, "scene_prev_tlas", as_manager->get_prev_tlas());
        }
        need_descriptor_set_update = false;
    }
}

void scene_stage::update_temporal_tables(uint32_t frame_index)
{
    if(!opt.build_temporal_tables)
        return;

    size_t alignment = dev->physical_device_props.properties.limits.minStorageBufferOffsetAlignment / sizeof(uint32_t);
    instance_forward_offset = 0;
    instance_backward_offset = align_up_to(instance_forward_offset+instance_prev_count, alignment);
    point_light_forward_offset = align_up_to(instance_backward_offset+instance_count, alignment);
    point_light_backward_offset = align_up_to(point_light_forward_offset+point_light_prev_count, alignment);
    sorted_point_light_forward_offset = align_up_to(point_light_backward_offset+point_light_count, alignment);
    sorted_point_light_backward_offset = align_up_to(sorted_point_light_forward_offset+point_light_prev_count, alignment);

    size_t total_size = align_up_to(sorted_point_light_backward_offset+point_light_count, alignment);

    auto total_mem = global_stack_allocator.allocate<uint32_t>(total_size);
    memset(total_mem.data(), 0xFF, total_mem.size() * sizeof(uint32_t));

    for(uint32_t i = 0; i < instance_count; ++i)
    {
        uint32_t prev_id = instance_prev_map[i];
        total_mem[instance_backward_offset+i] = prev_id;
        if(prev_id != 0xFFFFFFFFu && prev_id < instance_prev_count)
            total_mem[instance_forward_offset + prev_id] = i;
    }
    for(uint32_t i = 0; i < point_light_count; ++i)
    {
        uint32_t prev_id = unsorted_point_light_prev_map[i];
        total_mem[point_light_backward_offset+i] = prev_id;
        if(prev_id != 0xFFFFFFFFu && prev_id < point_light_prev_count)
            total_mem[point_light_forward_offset + prev_id] = i;
    }

    // The +4 is just to ensure that we don't try to resize it to zero.
    temporal_tables.resize(total_mem.size() * sizeof(uint32_t) + 4);
    temporal_tables.update_ptr(frame_index, total_mem.data(), total_mem.size() * sizeof(uint32_t));

    temporal_tables_set.reset(1);
    temporal_tables_set.set_buffer(0, "instance_map_forward", (VkBuffer)temporal_tables, instance_forward_offset * sizeof(uint32_t));
    temporal_tables_set.set_buffer(0, "instance_map_backward", (VkBuffer)temporal_tables, instance_backward_offset * sizeof(uint32_t));
    if(cluster_provider && cluster_provider->sorted_point_lights)
    {
        temporal_tables_set.set_buffer(0, "point_light_map_forward", (VkBuffer)temporal_tables, sorted_point_light_forward_offset * sizeof(uint32_t));
        temporal_tables_set.set_buffer(0, "point_light_map_backward", (VkBuffer)temporal_tables, sorted_point_light_backward_offset * sizeof(uint32_t));
    }
    else
    {
        temporal_tables_set.set_buffer(0, "point_light_map_forward", (VkBuffer)temporal_tables, point_light_forward_offset * sizeof(uint32_t));
        temporal_tables_set.set_buffer(0, "point_light_map_backward", (VkBuffer)temporal_tables, point_light_backward_offset * sizeof(uint32_t));
    }
    temporal_tables_set.set_buffer(0, "prev_point_lights", *prev_point_lights);
}

int32_t scene_stage::add_texture(scene_stage* self, material::sampler_tex tex, sampler* default_sampler)
{
    if(tex.second == nullptr) return -1;
    if(tex.first == nullptr) tex.first = default_sampler;
    return self->sampler_indices.insert(tex);
}

void scene_stage::run_animation_update(VkCommandBuffer cmd)
{
    if(animated_mesh_indices.size() == 0)
        return;

    animation_set.reset(1);
    animation_set.set_buffer(0, "vertex_position", vertex_pnt_buffers, pos_offsets);
    animation_set.set_buffer(0, "vertex_normal", vertex_pnt_buffers, normal_offsets);
    animation_set.set_buffer(0, "vertex_tangent", vertex_pnt_buffers, tangent_offsets);
    animation_set.set_buffer(0, "vertex_prev_position", vertex_pnt_buffers, prev_pos_offsets);
    animation_set.set_buffer(0, "morph_target_weights", (VkBuffer)morph_target_weights);
    animation_set.set_buffer(0, "vertex_joints", vertex_skeletal_buffers, joints_offsets);
    animation_set.set_buffer(0, "vertex_weights", vertex_skeletal_buffers, weights_offsets);
    animation_set.set_buffer(0, "joint_matrices", (VkBuffer)skeletal_joints);
    uint32_t dquat_offset = dualquat_joint_count == 0 ?
        0 : matrix_joint_count * sizeof(mat4);
    animation_set.set_buffer(0, "joint_dualquats", (VkBuffer)skeletal_joints, dquat_offset);

    animation_pipeline.bind(cmd);
    animation_pipeline.set_descriptors(cmd, animation_set);

    animation_push_constant_buffer pc;
    pc.morph_target_count = 0;
    pc.morph_target_weight_offset = 0;
    pc.vertex_count = 0;

    pc.base_buffer_index = 0;

    pc.morph_target_has_positions = 0;
    pc.morph_target_has_normals = 0;
    pc.morph_target_has_tangents = 0;

    pc.dst_position_index = 0;
    pc.dst_normal_index = 0;
    pc.dst_tangent_index = 0;
    pc.dst_prev_position_index = 0;

    pc.joint_count = 0;
    pc.joint_matrices_index = 0;
    pc.joint_dualquats_index = 0;

    int32_t joint_matrices_index = 0;
    int32_t joint_dualquats_index = 0;

    for(mesh* m: animated_mesh_indices)
    {
        const auto& mt_weights = m->get_morph_target_weights();
        size_t morph_target_count = std::count_if(
            mt_weights.begin(),
            mt_weights.end(),
            keep_morph_target
        );
        size_t first_non_zero_mt_index = std::find_if(
            mt_weights.begin(),
            mt_weights.end(),
            keep_morph_target
        ) - mt_weights.begin();

        skeleton* skel = m->get_skeleton();
        if(skel)
        {
            skeleton::skinning_mode skin = skel->get_skinning_mode();
            if(skin == skeleton::LINEAR)
            {
                pc.joint_matrices_index = joint_matrices_index;
                pc.joint_dualquats_index = -1;
                joint_matrices_index += skel->get_true_joint_count();
            }
            else if(skin == skeleton::DUAL_QUATERNION)
            {
                pc.joint_matrices_index = -1;
                pc.joint_dualquats_index = joint_dualquats_index;
                joint_dualquats_index += skel->get_true_joint_count();
            }
        }
        else
        {
            pc.joint_matrices_index = -1;
            pc.joint_dualquats_index = -1;
        }

        for(vertex_group& vg: *m)
        {
            pc.vertex_count = vg.source->get_vertex_count();

            pc.base_buffer_index = primitive_indices.get(vg.source);

            pc.morph_target_count = 0;
            if(vg.morph_targets.size() != 0 && morph_target_count != 0)
            {
                pc.morph_target_count = morph_target_count;
                pc.morph_target_has_positions = 1;
                pc.morph_target_has_normals =
                    vg.morph_targets[0]->has_attribute(primitive::NORMAL);
                pc.morph_target_has_tangents =
                    vg.morph_targets[0]->has_attribute(primitive::TANGENT);

                for(
                    size_t j = 0, k = 0;
                    j < vg.morph_targets.size() && k < MAX_ACTIVE_MORPH_TARGETS_PER_MESH;
                    ++j
                ){
                    if(keep_morph_target(mt_weights[j]))
                    {
                        pc.morph_target_ptn_indices[k] =
                            primitive_indices.get(vg.morph_targets[j]);
                        k++;
                    }
                }
            }
            else
            {
                pc.morph_target_has_positions = 0;
                pc.morph_target_has_normals = 0;
                pc.morph_target_has_tangents = 0;
            }

            if(vg.source->has_attribute(primitive::JOINTS) && skel)
                pc.joint_count = skel->get_true_joint_count();
            else
                pc.joint_count = 0;

            pc.dst_position_index = primitive_indices.get(
                vg.animation_primitive.get()
            );
            pc.dst_normal_index =
                vg.animation_primitive->has_attribute(primitive::NORMAL) ?
                pc.dst_position_index : -1;
            pc.dst_tangent_index =
                vg.animation_primitive->has_attribute(primitive::TANGENT) ?
                pc.dst_position_index : -1;
            pc.dst_prev_position_index = pc.dst_position_index;

            animation_pipeline.push_constants(cmd, &pc);
            animation_pipeline.dispatch(cmd, uvec3((pc.vertex_count + 63u)/64u, 1, 1));
        }
        m->mark_animation_refreshed();
        pc.morph_target_weight_offset += morph_target_count;
    }
}

void scene_stage::run_tri_light_update(VkCommandBuffer cmd)
{
    if(tri_light_count == 0)
        return;

    // Barrier for previous animation updates
    VkMemoryBarrier2KHR barrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
        nullptr,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        VK_ACCESS_2_MEMORY_READ_BIT_KHR
    };
    VkDependencyInfoKHR deps = {
        VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, nullptr, 0,
        1, &barrier, 0, nullptr, 0, nullptr
    };
    vkCmdPipelineBarrier2KHR(cmd, &deps);

    tri_light_set.set_buffer("out_tri_lights", (VkBuffer)tri_lights);

    tri_light_pipeline.bind(cmd);
    tri_light_pipeline.push_descriptors(cmd, tri_light_set, 0);
    tri_light_pipeline.set_descriptors(cmd, scene_data_set, 0, 1);

    size_t offset = 0;
    for(render_entry entry: generic_render_list)
    {
        model* mod = current_scene->get<model>(entry.id);
        material& mat = mod->materials[entry.vertex_group_index];
        if(!mat.potentially_emissive())
            continue;

        size_t tri_count = (*mod->m)[entry.vertex_group_index].get_primitive()->get_index_count()/3;
        tri_light_push_constant_buffer pc;
        pc.triangle_offset = offset;
        pc.triangle_count = tri_count;
        pc.instance_id = entry.instance_index;
        offset += tri_count;

        tri_light_pipeline.push_constants(cmd, &pc);
        tri_light_pipeline.dispatch(cmd, uvec3((tri_count+255u)/256u, 1, 1));
    }
}

}
