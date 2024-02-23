#ifndef RAYBASE_GFX_SCENE_GLSL
#define RAYBASE_GFX_SCENE_GLSL

layout(constant_id = 0) const uint RB_SUPPORTED_MATERIAL_FEATURES = 0xFFFFFFFF;
layout(constant_id = 1) const uint RB_LIGHT_CLUSTER_SLICE_ENTRIES = 0;
layout(constant_id = 2) const uint RB_LIGHT_HIERARCHY_SLICE_ENTRIES = 0;
layout(constant_id = 3) const uint RB_DECAL_CLUSTER_SLICE_ENTRIES = 0;
layout(constant_id = 4) const uint RB_DECAL_HIERARCHY_SLICE_ENTRIES = 0;

#include "material_data.glsl"
#include "color.glsl"
#include "decal.glsl"

#define ATTRIB_POSITION (1u<<0u)
#define ATTRIB_NORMAL (1u<<1u)
#define ATTRIB_TANGENT (1u<<2u)
#define ATTRIB_TEXTURE_UV (1u<<3u)
#define ATTRIB_LIGHTMAP_UV (1u<<4u)
#define ATTRIB_JOINTS (1u<<5u)
#define ATTRIB_WEIGHTS (1u<<6u)
#define ATTRIB_COLOR (1u<<7u)
#define ATTRIB_PREV_POSITION (1u<<8u)

#ifndef RAYBASE_SCENE_SET
#define RAYBASE_SCENE_SET 1
#endif

#ifndef RAYBASE_SCENE_ACCESS
#define RAYBASE_SCENE_ACCESS readonly
#endif

#ifdef RAYBASE_RAY_CONE_GRADIENTS
#define RAYBASE_VERTEX_DATA_TRIANGLE_POS
#define RAYBASE_VERTEX_DATA_TRIANGLE_UV
#endif

struct vertex_data
{
    vec3 pos;
#ifdef RAYBASE_FETCH_PREV_POS
    vec3 prev_pos;
#endif
    vec4 uv;
#ifdef RAYBASE_VERTEX_DATA_TRIANGLE_POS
    vec3 triangle_pos[3];
#endif
#ifdef RAYBASE_VERTEX_DATA_TRIANGLE_UV
    vec2 triangle_uv[3];
#endif
    vec3 flat_normal;
    vec3 smooth_normal;
    mat3 tangent_space;
};

// Values for the 'flags' entry of 'struct instance'.
const uint INSTANCE_FLAG_RECEIVES_DECALS = 1<<0;
const uint INSTANCE_FLAG_HAS_TRI_LIGHTS = 1<<1;

struct instance
{
    mat4 model_to_world;
    mat4 normal_to_world;
    mat4 prev_model_to_world;
    material_spec material;
    // x = radiance cubemap index, y = irradiance cubemap index, z = lightmap index, w = unused
    ivec4 environment;
    uint buffer_index;
    uint attribute_mask;
    uint flags;
    uint triangle_count;
};

struct camera
{
    mat4 view_proj;
    mat4 inv_view;
    mat4 inv_view_proj;
    mat4 prev_view_proj;
    mat4 prev_inv_view;
    vec4 proj_info;
    vec4 prev_proj_info;
    float focal_distance;
    float aperture_radius;
    float aperture_angle;
    float pad[1];
};

#define FILM_FILTER_POINT 0
#define FILM_FILTER_BOX 1
#define FILM_FILTER_GAUSSIAN 2
#define FILM_FILTER_BLACKMAN_HARRIS 3

layout(binding = 0, set = RAYBASE_SCENE_SET) uniform sampler2D textures[];

layout(binding = 1, set = RAYBASE_SCENE_SET) uniform samplerCube cube_textures[];

#include "light.glsl"

layout(binding = 2, set = RAYBASE_SCENE_SET) RAYBASE_SCENE_ACCESS buffer instance_buffer
{
    instance array[];
} instances;

layout(binding = 3, set = RAYBASE_SCENE_SET) RAYBASE_SCENE_ACCESS buffer point_light_buffer
{
    point_light array[];
} point_lights;

layout(binding = 4, set = RAYBASE_SCENE_SET) RAYBASE_SCENE_ACCESS buffer directional_light_buffer
{
    directional_light array[];
} directional_lights;

layout(binding = 5, set = RAYBASE_SCENE_SET) RAYBASE_SCENE_ACCESS buffer tri_light_buffer
{
    tri_light array[];
} tri_lights;

layout(binding = 6, set = RAYBASE_SCENE_SET) RAYBASE_SCENE_ACCESS buffer decal_buffer
{
    decal array[];
} decals;

layout(binding = 7, set = RAYBASE_SCENE_SET) RAYBASE_SCENE_ACCESS buffer camera_buffer
{
    camera array[];
} cameras;

layout(binding = 8, set = RAYBASE_SCENE_SET, scalar) RAYBASE_SCENE_ACCESS buffer position_buffer
{
    vec3 array[];
} vertex_position[];

#ifdef RAYBASE_FETCH_PREV_POS
layout(binding = 9, set = RAYBASE_SCENE_SET, scalar) RAYBASE_SCENE_ACCESS buffer prev_position_buffer
{
    vec3 array[];
} vertex_prev_position[];
#endif

layout(binding = 10, set = RAYBASE_SCENE_SET, scalar) RAYBASE_SCENE_ACCESS buffer normal_buffer
{
    vec3 array[];
} vertex_normal[];

layout(binding = 11, set = RAYBASE_SCENE_SET, scalar) RAYBASE_SCENE_ACCESS buffer tangent_buffer
{
    vec4 array[];
} vertex_tangent[];

layout(binding = 12, set = RAYBASE_SCENE_SET, scalar) RAYBASE_SCENE_ACCESS buffer texture_uv_buffer
{
    vec2 array[];
} vertex_texture_uv[];

layout(binding = 13, set = RAYBASE_SCENE_SET, scalar) RAYBASE_SCENE_ACCESS buffer lightmap_uv_buffer
{
    vec2 array[];
} vertex_lightmap_uv[];

layout(binding = 14, set = RAYBASE_SCENE_SET) RAYBASE_SCENE_ACCESS buffer index_buffer
{
    uint array[];
} indices[];

layout(binding = 15, set = RAYBASE_SCENE_SET) RAYBASE_SCENE_ACCESS buffer light_cluster_buffer
{
    uvec4 slices[];
} light_cluster_slices;

layout(binding = 16, set = RAYBASE_SCENE_SET) RAYBASE_SCENE_ACCESS buffer decal_cluster_buffer
{
    uvec4 slices[];
} decal_cluster_slices;

layout(binding = 17, set = RAYBASE_SCENE_SET, scalar) uniform scene_param_buffer
{
    ivec4 light_cluster_size;
    vec4 light_cluster_inv_slice;
    vec4 light_cluster_offset;
    uvec4 light_cluster_axis_offsets;
    uvec4 light_hierarchy_axis_offsets;
    ivec4 decal_cluster_size;
    vec4 decal_cluster_inv_slice;
    vec4 decal_cluster_offset;
    uvec4 decal_cluster_axis_offsets;
    uvec4 decal_hierarchy_axis_offsets;
    vec4 envmap_orientation;
    int envmap_index;
    uint envmap_face_size_x;
    uint envmap_face_size_y;
    uint point_light_count;
    uint directional_light_count;
    uint tri_light_count;
    uint decal_count;
    uint instance_count;
} scene_params;

layout(binding = 18, set = RAYBASE_SCENE_SET) uniform sampler2D material_lut;

layout(binding = 22, set = RAYBASE_SCENE_SET) uniform sampler2D blue_noise_lut;
layout(binding = 23, set = RAYBASE_SCENE_SET) RAYBASE_SCENE_ACCESS buffer envmap_metadata_buffer
{
    mat4 world_to_envmap[];
} envmap_metadata;

#ifdef RAYBASE_RAY_TRACING
layout(binding = 27, set = RAYBASE_SCENE_SET) uniform accelerationStructureEXT scene_tlas;
// When using the previous TLAS, please keep in mind that the instance indices
// may have changed since then -- DON'T USE THEM! This means that you MUST use
// gl_RayFlagsOpaqueEXT with scene_prev_tlas, to avoid any hit shaders which
// may try to read textures! This should pretty much only be used for
// visibility tests.
layout(binding = 28, set = RAYBASE_SCENE_SET) uniform accelerationStructureEXT scene_prev_tlas;

#define TLAS_DYNAMIC_MESH_MASK (1u<<0u)
#define TLAS_STATIC_MESH_MASK (1u<<1u)
#define TLAS_LIGHT_MASK (1u<<2u)
#endif

#include "material.glsl"

#define POINT_LIGHT_MASK_SYNC(mask)
#define FOR_POINT_LIGHTS(name, world_pos)  \
    FOR_CLUSTER( \
        world_pos, \
        scene_params.light_cluster_inv_slice.xyz, \
        scene_params.light_cluster_offset.xyz, \
        scene_params.light_cluster_size.xyz, \
        scene_params.light_cluster_axis_offsets.xyz, \
        scene_params.light_hierarchy_axis_offsets.xyz, \
        RB_LIGHT_CLUSTER_SLICE_ENTRIES, \
        RB_LIGHT_HIERARCHY_SLICE_ENTRIES, \
        light_cluster_slices, \
        POINT_LIGHT_MASK_SYNC \
    ) \
    point_light name = point_lights.array[item_index];

#define END_POINT_LIGHTS END_FOR_CLUSTER

// Returns considered light count.
int pick_random_point_light_in_range(
    vec3 world_pos,
    out point_light light,
    inout uint seed
){
    int selected_index = -1;
    int item_count = 0;
    SAMPLE_CLUSTER(
        world_pos,
        scene_params.light_cluster_inv_slice.xyz,
        scene_params.light_cluster_offset.xyz,
        scene_params.light_cluster_size.xyz,
        scene_params.light_cluster_axis_offsets.xyz,
        scene_params.light_hierarchy_axis_offsets.xyz,
        RB_LIGHT_CLUSTER_SLICE_ENTRIES,
        RB_LIGHT_HIERARCHY_SLICE_ENTRIES,
        light_cluster_slices,
        seed,
        POINT_LIGHT_MASK_SYNC
    );
    if(selected_index >= 0)
        light = point_lights.array[selected_index];
    return item_count;
}

int get_point_light_count_in_range(vec3 world_pos)
{
    int item_count = 0;
    GET_CLUSTER_COUNT(
        world_pos,
        scene_params.light_cluster_inv_slice.xyz,
        scene_params.light_cluster_offset.xyz,
        scene_params.light_cluster_size.xyz,
        scene_params.light_cluster_axis_offsets.xyz,
        scene_params.light_hierarchy_axis_offsets.xyz,
        RB_LIGHT_CLUSTER_SLICE_ENTRIES,
        RB_LIGHT_HIERARCHY_SLICE_ENTRIES,
        light_cluster_slices,
        POINT_LIGHT_MASK_SYNC
    );
    return item_count;
}

#ifdef IMPLICIT_GRADIENT_SAMPLING
// Implicit gradients will be fucked up at the edges of decals due to the
// clustering edge passing through there. However, we can fix this! By abusing
// subgroups and ensuring that all pixels in the subgroup go through the same
// decals. Implicit gradients are also calculated within the subgroup :-D
#define DECAL_MASK_SYNC(mask) mask = subgroupOr(mask);
#else
#define DECAL_MASK_SYNC(mask)
#endif
#define FOR_DECALS(name, world_pos)  \
    FOR_CLUSTER( \
        world_pos, \
        scene_params.decal_cluster_inv_slice.xyz, \
        scene_params.decal_cluster_offset.xyz, \
        scene_params.decal_cluster_size.xyz, \
        scene_params.decal_cluster_axis_offsets.xyz, \
        scene_params.decal_hierarchy_axis_offsets.xyz, \
        RB_DECAL_CLUSTER_SLICE_ENTRIES, \
        RB_DECAL_HIERARCHY_SLICE_ENTRIES, \
        decal_cluster_slices, \
        DECAL_MASK_SYNC \
    ) \
    decal name = decals.array[item_index];

#define END_DECALS END_FOR_CLUSTER

#define FOR_DIRECTIONAL_LIGHTS(name) \
    for(uint i = 0; i < scene_params.directional_light_count; ++i) \
    { \
        uint item_index = i; \
        directional_light name = directional_lights.array[i];
#define END_DIRECTIONAL_LIGHTS \
    }

vertex_data zero_vertex_data()
{
    vertex_data vd;
    vd.pos = vec3(0);
#ifdef RAYBASE_FETCH_PREV_POS
    vd.prev_pos = vec3(0);
#endif
    vd.uv = vec4(0);
#ifdef RAYBASE_VERTEX_DATA_TRIANGLE_POS
    vd.triangle_pos[0] = vec3(0);
    vd.triangle_pos[1] = vec3(0);
    vd.triangle_pos[2] = vec3(0);
#endif
#ifdef RAYBASE_VERTEX_DATA_TRIANGLE_UV
    vd.triangle_uv[0] = vec2(0);
    vd.triangle_uv[1] = vec2(0);
    vd.triangle_uv[2] = vec2(0);
#endif
    vd.flat_normal = vec3(0);
    vd.smooth_normal = vec3(0);
    vd.tangent_space = mat3(0);
    return vd;
}

vertex_data get_vertex_data(
    uint instance_index,
    uint primitive,
    vec2 barycentric
#ifdef RAYBASE_VERTEX_DATA_TRIANGLE_POS
    , bool calculate_triangle_pos
#endif
){
    // If you get intermittent freezes, uncomment these to see if it's caused
    // by bad instance / primitive IDs.
    //if(instance_index >= scene_params.instance_count)
    //    return zero_vertex_data();
    instance i = instances.array[nonuniformEXT(instance_index)];

    //if(primitive >= i.triangle_count)
    //    return zero_vertex_data();

    uint mesh = i.buffer_index;

    uint index0 = indices[nonuniformEXT(mesh)].array[3*primitive+0];
    uint index1 = indices[nonuniformEXT(mesh)].array[3*primitive+1];
    uint index2 = indices[nonuniformEXT(mesh)].array[3*primitive+2];

    vec3 pos0 = vertex_position[nonuniformEXT(mesh)].array[index0];
    vec3 pos1 = vertex_position[nonuniformEXT(mesh)].array[index1];
    vec3 pos2 = vertex_position[nonuniformEXT(mesh)].array[index2];

#ifdef RAYBASE_FETCH_PREV_POS
    vec3 prev_pos0 = vertex_prev_position[nonuniformEXT(mesh)].array[index0];
    vec3 prev_pos1 = vertex_prev_position[nonuniformEXT(mesh)].array[index1];
    vec3 prev_pos2 = vertex_prev_position[nonuniformEXT(mesh)].array[index2];
#endif

    vec3 flat_normal = cross(pos0-pos1, pos0-pos2);

    vec3 normal0 = flat_normal;
    vec3 normal1 = flat_normal;
    vec3 normal2 = flat_normal;

    if((i.attribute_mask & ATTRIB_NORMAL) != 0)
    {
        normal0 = vertex_normal[nonuniformEXT(mesh)].array[index0];
        normal1 = vertex_normal[nonuniformEXT(mesh)].array[index1];
        normal2 = vertex_normal[nonuniformEXT(mesh)].array[index2];
    }

    vec4 tangent0 = vec4(0,0,0,1);
    vec4 tangent1 = vec4(0,0,0,1);
    vec4 tangent2 = vec4(0,0,0,1);
    if((i.attribute_mask & ATTRIB_TANGENT) != 0)
    {
        tangent0 = vertex_tangent[nonuniformEXT(mesh)].array[index0];
        tangent1 = vertex_tangent[nonuniformEXT(mesh)].array[index1];
        tangent2 = vertex_tangent[nonuniformEXT(mesh)].array[index2];
    }
    else
    {
        // We want to always have a valid tangent space.
        tangent0.xyz = create_tangent(normal0);
        tangent1.xyz = create_tangent(normal1);
        tangent2.xyz = create_tangent(normal2);
    }

    vec4 uv0 = vec4(0);
    vec4 uv1 = vec4(0);
    vec4 uv2 = vec4(0);

    if((i.attribute_mask & ATTRIB_TEXTURE_UV) != 0)
    {
        uv0.xy = vertex_texture_uv[nonuniformEXT(mesh)].array[index0];
        uv1.xy = vertex_texture_uv[nonuniformEXT(mesh)].array[index1];
        uv2.xy = vertex_texture_uv[nonuniformEXT(mesh)].array[index2];
    }

    if((i.attribute_mask & ATTRIB_LIGHTMAP_UV) != 0)
    {
        uv0.zw = vertex_lightmap_uv[nonuniformEXT(mesh)].array[index0];
        uv1.zw = vertex_lightmap_uv[nonuniformEXT(mesh)].array[index1];
        uv2.zw = vertex_lightmap_uv[nonuniformEXT(mesh)].array[index2];
    }

    vec3 weights = vec3(1.0f - barycentric.x - barycentric.y, barycentric);

    vec3 model_pos = pos0.xyz * weights.x + pos1.xyz * weights.y + pos2.xyz * weights.z;
#ifdef RAYBASE_FETCH_PREV_POS
    vec3 model_prev_pos = prev_pos0.xyz * weights.x + prev_pos1.xyz * weights.y + prev_pos2.xyz * weights.z;
#endif
    vec3 model_normal = normal0.xyz * weights.x + normal1.xyz * weights.y + normal2.xyz * weights.z;
    vec4 model_uv = uv0 * weights.x + uv1 * weights.y + uv2 * weights.z;
    vec4 model_tangent = tangent0 * weights.x + tangent1 * weights.y + tangent2 * weights.z;

    vertex_data vd;
    vd.pos = vec3(i.model_to_world * vec4(model_pos, 1));
#ifdef RAYBASE_FETCH_PREV_POS
    vd.prev_pos = vec3(i.prev_model_to_world * vec4(model_prev_pos, 1));
#endif
    vd.uv = model_uv;
    vd.flat_normal = normalize(mat3(i.model_to_world) * flat_normal);
    vd.smooth_normal = normalize(mat3(i.model_to_world) * model_normal);
    vd.tangent_space[2] = vd.smooth_normal;
    vd.tangent_space[0] = normalize(mat3(i.model_to_world) * model_tangent.xyz);
    if(abs(dot(vd.tangent_space[0], vd.tangent_space[2])) > 0.9999f)
        vd.tangent_space[0] = create_tangent(vd.tangent_space[2]);
    vd.tangent_space[1] = normalize(cross(vd.tangent_space[2], vd.tangent_space[0]) * model_tangent.w);

#ifdef RAYBASE_VERTEX_DATA_TRIANGLE_POS
    if(calculate_triangle_pos)
    {
        vd.triangle_pos[0] = vec3(i.model_to_world * vec4(pos0, 1));
        vd.triangle_pos[1] = vec3(i.model_to_world * vec4(pos1, 1));
        vd.triangle_pos[2] = vec3(i.model_to_world * vec4(pos2, 1));
    }
#endif
#ifdef RAYBASE_VERTEX_DATA_TRIANGLE_UV
    vd.triangle_uv[0] = uv0.xy;
    vd.triangle_uv[1] = uv1.xy;
    vd.triangle_uv[2] = uv2.xy;
#endif

    return vd;
}

void get_vertex_position(
    uint instance_index,
    uint primitive,
    vec2 barycentric,
    out vec3 position,
    out vec3 flat_normal,
    out float nee_pdf,
    vec3 domain_pos
){
    /*
    if(instance_index >= scene_params.instance_count)
    {
        position = vec3(0);
        flat_normal = vec3(0);
        return;
    }
    */

    instance i = instances.array[nonuniformEXT(instance_index)];

    /*
    if(primitive >= i.triangle_count)
    {
        position = vec3(0);
        flat_normal = vec3(0);
        return;
    }
    */

    uint mesh = i.buffer_index;

    uint index0 = indices[nonuniformEXT(mesh)].array[3*primitive+0];
    uint index1 = indices[nonuniformEXT(mesh)].array[3*primitive+1];
    uint index2 = indices[nonuniformEXT(mesh)].array[3*primitive+2];

    vec3 pos0 = vertex_position[nonuniformEXT(mesh)].array[index0];
    vec3 pos1 = vertex_position[nonuniformEXT(mesh)].array[index1];
    vec3 pos2 = vertex_position[nonuniformEXT(mesh)].array[index2];

    if((i.flags & INSTANCE_FLAG_HAS_TRI_LIGHTS) != 0)
    {
        vec3 triangle_pos0 = vec3(i.model_to_world * vec4(pos0, 1));
        vec3 triangle_pos1 = vec3(i.model_to_world * vec4(pos1, 1));
        vec3 triangle_pos2 = vec3(i.model_to_world * vec4(pos2, 1));
        nee_pdf = 1.0f / triangle_solid_angle(
            domain_pos,
            triangle_pos0,
            triangle_pos1,
            triangle_pos2
        );
    }

    vec3 weights = vec3(1.0f - barycentric.x - barycentric.y, barycentric);

    vec3 model_pos = pos0.xyz * weights.x + pos1.xyz * weights.y + pos2.xyz * weights.z;

    flat_normal = normalize(mat3(i.model_to_world) * cross(pos0-pos1, pos0-pos2));
    position = vec3(i.model_to_world * vec4(model_pos, 1));
}

void get_vertex_prev_position(
    uint instance_index,
    uint primitive,
    vec2 barycentric,
    out vec3 cur_position,
    out vec3 cur_flat_normal,
    out float cur_nee_pdf,
    out vec3 prev_position,
    out vec3 prev_flat_normal,
    out float prev_nee_pdf,
    vec3 domain_pos
){
    instance i = instances.array[nonuniformEXT(instance_index)];
    uint mesh = i.buffer_index;

    uint index0 = indices[nonuniformEXT(mesh)].array[3*primitive+0];
    uint index1 = indices[nonuniformEXT(mesh)].array[3*primitive+1];
    uint index2 = indices[nonuniformEXT(mesh)].array[3*primitive+2];

    vec3 pos0 = vertex_position[nonuniformEXT(mesh)].array[index0];
    vec3 pos1 = vertex_position[nonuniformEXT(mesh)].array[index1];
    vec3 pos2 = vertex_position[nonuniformEXT(mesh)].array[index2];

    if((i.flags & INSTANCE_FLAG_HAS_TRI_LIGHTS) != 0)
    {
        vec3 triangle_pos0 = vec3(i.model_to_world * vec4(pos0, 1));
        vec3 triangle_pos1 = vec3(i.model_to_world * vec4(pos1, 1));
        vec3 triangle_pos2 = vec3(i.model_to_world * vec4(pos2, 1));
        cur_nee_pdf = 1.0f / triangle_solid_angle(
            domain_pos,
            triangle_pos0,
            triangle_pos1,
            triangle_pos2
        );
        triangle_pos0 = vec3(i.prev_model_to_world * vec4(pos0, 1));
        triangle_pos1 = vec3(i.prev_model_to_world * vec4(pos1, 1));
        triangle_pos2 = vec3(i.prev_model_to_world * vec4(pos2, 1));
        prev_nee_pdf = 1.0f / triangle_solid_angle(
            domain_pos,
            triangle_pos0,
            triangle_pos1,
            triangle_pos2
        );
    }

    vec3 weights = vec3(1.0f - barycentric.x - barycentric.y, barycentric);

    vec3 model_pos = pos0.xyz * weights.x + pos1.xyz * weights.y + pos2.xyz * weights.z;

    cur_flat_normal = normalize(mat3(i.model_to_world) * cross(pos0-pos1, pos0-pos2));
    cur_position = vec3(i.model_to_world * vec4(model_pos, 1));
    prev_flat_normal = normalize(mat3(i.prev_model_to_world) * cross(pos0-pos1, pos0-pos2));
    prev_position = vec3(i.prev_model_to_world * vec4(model_pos, 1));
}

#ifdef RAYBASE_RAY_CONE_GRADIENTS
// Improved Shader and Texture Level of Detail Using Ray Cones
// https://www.jcgt.org/published/0010/01/01/paper-lowres.pdf
//
// Should update the ray cone, but it's kinda WIP and doesn't take curvature
// or roughness into account
void ray_cone_intersection(
    vec3 ray_origin,
    vec3 ray_dir,
    inout float ray_cone_angle,
    inout float ray_cone_radius,
    vertex_data vd,
    out vec2 puvdx,
    out vec2 puvdy
){
    ray_cone_radius += ray_cone_angle * distance(ray_origin, vd.pos);

    vec3 a1 = ray_dir - dot(vd.flat_normal, ray_dir) * vd.flat_normal;
    vec3 p1 = a1 - dot(ray_dir, a1) * ray_dir;
    a1 *= ray_cone_radius / max(0.0001, length(p1));

    vec3 a2 = cross(vd.flat_normal, a1);
    vec3 p2 = a2 - dot(ray_dir, a2) * ray_dir;
    a2 *= ray_cone_radius / max(0.0001, length(p2));

    vec3 delta = vd.pos - vd.triangle_pos[0];
    vec3 e1 = vd.triangle_pos[1] - vd.triangle_pos[0];
    vec3 e2 = vd.triangle_pos[2] - vd.triangle_pos[0];
    float inv_area = 1.0f / dot(vd.flat_normal, cross(e1, e2));

    vec3 eP = delta + a1;
    vec2 a1_bary = vec2(
        dot(vd.flat_normal, cross(eP, e2)),
        dot(vd.flat_normal, cross(e1, eP))
    ) * inv_area;

    puvdx = (1.0f - a1_bary.x - a1_bary.y) * vd.triangle_uv[0].xy + a1_bary.x * vd.triangle_uv[1].xy + a1_bary.y * vd.triangle_uv[2].xy - vd.uv.xy;

    eP = delta + a2;
    vec2 a2_bary = vec2(
        dot(vd.flat_normal, cross(eP, e2)),
        dot(vd.flat_normal, cross(e1, eP))
    ) * inv_area;

    puvdy = (1.0f - a2_bary.x - a2_bary.y) * vd.triangle_uv[0].xy + a2_bary.x * vd.triangle_uv[1].xy + a2_bary.y * vd.triangle_uv[2].xy - vd.uv.xy;
}
#endif

vec3 unproject_position(float depth, vec2 uv, in camera cam)
{
    return unproject_position(depth, vec2(uv.x, 1-uv.y), cam.proj_info);
}

// Used for edge detection algorithms. Inverse size of frustum at the depth of
// a given point.
float get_frustum_size(camera cam, vec3 pos)
{
    float frustum_size = min(abs(cam.proj_info.z), abs(cam.proj_info.w));
    if(cam.proj_info.x < 0)
    { // Perspective
        frustum_size *= abs(dot(cam.inv_view[2].xyz, cam.inv_view[3].xyz - pos));
    }

    return frustum_size;
}

float get_prev_frustum_size(camera cam, vec3 pos)
{
    float frustum_size = min(abs(cam.prev_proj_info.z), abs(cam.prev_proj_info.w));
    if(cam.proj_info.x < 0)
    { // Perspective
        frustum_size *= abs(dot(cam.prev_inv_view[2].xyz, cam.prev_inv_view[3].xyz - pos));
    }

    return frustum_size;
}

void get_pinhole_camera_ray(
    vec2 uv,
    in camera cam,
    out vec3 origin,
    out vec3 dir
){
    origin = cam.inv_view[3].xyz;
    vec3 target = vec3(vec2(uv.x-0.5f, 0.5f-uv.y) * cam.proj_info.zw, -1);
    dir = normalize((cam.inv_view * vec4(target, 0.0f)).xyz);
}

void get_thin_lens_camera_ray(
    vec2 uv,
    in camera cam,
    vec2 aperture_pos,
    out vec3 origin,
    out vec3 dir
){
    vec2 o = aperture_pos * cam.aperture_radius;
    origin = vec3(o, 0.0f);
    dir = vec3(vec2(uv.x-0.5f, 0.5f-uv.y) * cam.proj_info.zw, -1) * cam.focal_distance;
    dir = normalize(dir - origin);

    // Move to world space
    origin = (cam.inv_view * vec4(origin, 1.0f)).xyz;
    dir = normalize((cam.inv_view * vec4(dir, 0.0f)).xyz);
}

vec3 get_camera_origin(in camera cam)
{
    return cam.inv_view[3].xyz;
}

#ifdef IMPLICIT_GRADIENT_SAMPLING
#define SAMPLE_TEXTURE_GRADIENT(uv) dFdx(uv), dFdy(uv)
#else
#define SAMPLE_TEXTURE_GRADIENT(uv) puvdx, puvdy
#endif

material sample_material_core(material_spec spec, bool front_facing, vec2 uv, vec2 puvdx, vec2 puvdy)
{
#define SAMPLE_TEXTURE(tex_id) textureGrad(textures[nonuniformEXT(tex_id)], uv, puvdx, puvdy)
    const uint albedo_texture = spec.color_metallic_roughness_textures & 0xFFFF;
    const uint metallic_roughness_texture = spec.color_metallic_roughness_textures >> 16;
    const uint normal_texture = spec.normal_emission_textures & 0xFFFF;
    const uint emission_texture = spec.normal_emission_textures >> 16;
    const uint clearcoat_texture = spec.clearcoat_textures & 0xFFFF;
    const uint clearcoat_normal_texture = spec.clearcoat_textures >> 16;
    const uint transmission_texture = spec.transmission_sheen_textures & 0xFFFF;
    const uint sheen_texture = spec.transmission_sheen_textures >> 16;

    material mat;
    if(HAS_MATERIAL(ALBEDO))
    {
        mat.albedo = unpackUnorm4x8(spec.color);
        if(albedo_texture != UNUSED_TEXTURE)
        {
            vec4 tex_col = SAMPLE_TEXTURE(albedo_texture);
            tex_col.rgb = inverse_srgb_correction(tex_col.rgb);
            mat.albedo *= tex_col;
        }
    }
    else
    {
        mat.albedo = vec4(1);
    }

    vec4 mrtt = unpackUnorm4x8(spec.metallic_roughness_transmission_translucency);
    if(HAS_MATERIAL(METALLIC_ROUGHNESS))
    {
        if(metallic_roughness_texture != UNUSED_TEXTURE)
            mrtt.xy *= SAMPLE_TEXTURE(metallic_roughness_texture).bg;
        mat.metallic = mrtt.x;
        mat.roughness = mrtt.y;
    }
    else
    {
        mat.metallic = 0.0f;
        mat.roughness = 1.0f;
    }
    mat.roughness *= mat.roughness;

    if(HAS_MATERIAL(TRANSMISSION))
    {
        if(transmission_texture != UNUSED_TEXTURE)
            mrtt.zw *= SAMPLE_TEXTURE(transmission_texture).rg;
        mat.transmission = mrtt.z;
        mat.translucency = mrtt.w;
    }
    else
    {
        mat.transmission = 0.0f;
        mat.translucency = 0.0f;
    }

    if(HAS_MATERIAL(EMISSION))
    {
        mat.emission = rgbe_to_rgb(spec.emission);
        if(emission_texture != UNUSED_TEXTURE)
        {
            vec3 tex_col = SAMPLE_TEXTURE(emission_texture).rgb;
            tex_col = inverse_srgb_correction(tex_col);
            mat.emission *= tex_col;
        }
        mat.emission *= mat.albedo.a;
    }
    else mat.emission = vec3(0);

    vec4 aior = unpackUnorm4x8(spec.attenuation_ior);
    if(HAS_MATERIAL(ATTENUATION)) mat.attenuation = aior.rgb;
    else mat.attenuation = vec3(1.0f);

    float ior = aior.a * 2.0f + 1.0f;
    mat.eta = front_facing ? 1.0 / ior : ior;
    float f0 = (1 - mat.eta)/(1 + mat.eta);
    f0 *= f0;
    mat.f0 = f0;

    vec4 cfra = unpackUnorm4x8(spec.clearcoat_factor_roughness_anisotropy);
    if(HAS_MATERIAL(CLEARCOAT))
    {
        if(clearcoat_texture != UNUSED_TEXTURE)
            cfra.rg *= SAMPLE_TEXTURE(clearcoat_texture).rg;
        mat.clearcoat = cfra.r;
        mat.clearcoat_roughness = cfra.g;
        mat.clearcoat_normal = vec3(0,0,1);
        if(HAS_MATERIAL(NORMAL_MAP))
        {
            if(clearcoat_normal_texture != UNUSED_TEXTURE)
            {
                vec3 ts_normal = SAMPLE_TEXTURE(clearcoat_normal_texture).xyz * 2.0f - 1.0f;
                mat.clearcoat_normal = normalize(ts_normal);
            }
        }
        mat.clearcoat_roughness *= mat.clearcoat_roughness;
    }
    else
    {
        mat.clearcoat = 0.0f;
        mat.clearcoat_roughness = 1.0f;
        mat.clearcoat_normal = vec3(0,0,1);
    }
    mat.clearcoat_normal = front_facing ? mat.clearcoat_normal : -mat.clearcoat_normal;

    if(HAS_MATERIAL(ANISOTROPY))
    {
        mat.anisotropy = cfra.b;
        mat.anisotropy_rotation = cfra.a * M_PI;
    }
    else
    {
        mat.anisotropy = 0.0f;
        mat.anisotropy_rotation = 0.0f;
    }

    vec4 scr = unpackUnorm4x8(spec.sheen_color_roughness);
    if(HAS_MATERIAL(SHEEN))
    {
        if(sheen_texture != UNUSED_TEXTURE)
        {
            vec4 tex_col = SAMPLE_TEXTURE(sheen_texture);
            tex_col.rgb = inverse_srgb_correction(tex_col.rgb);
            scr *= tex_col;
        }
        mat.sheen_color = scr.rgb;
        mat.sheen_roughness = max(scr.a, 0.07);
        mat.sheen_roughness *= mat.sheen_roughness;
    }
    else
    {
        mat.sheen_color = vec3(0);
        mat.sheen_roughness = 1.0f;
    }

    mat.normal = vec3(0,0,1);
    if(HAS_MATERIAL(NORMAL_MAP))
    {
        if(normal_texture != UNUSED_TEXTURE)
        {
            vec3 ts_normal = SAMPLE_TEXTURE(normal_texture).xyz * 2.0f - 1.0f;
            mat.normal = normalize(ts_normal);
        }
    }
    mat.normal = front_facing ? mat.normal : -mat.normal;

    vec4 dsc = unpackUnorm4x8(spec.double_sided_cutoff);
    float alpha_cutoff = dsc.y;
    if(alpha_cutoff != 1.0f)
    {
        if(mat.albedo.a < alpha_cutoff) mat.albedo.a = 0;
        else mat.albedo.a = 1;
    }

    return mat;
#undef SAMPLE_TEXTURE
}

material sample_material(
    material_spec spec, bool front_facing, vec2 uv
#ifndef IMPLICIT_GRADIENT_SAMPLING
    , vec2 puvdx, vec2 puvdy
#endif
){
    return sample_material_core(spec, front_facing, uv, SAMPLE_TEXTURE_GRADIENT(uv));
}

float sample_material_alpha_core(
    material_spec spec, vec2 uv, vec2 puvdx, vec2 puvdy
){
    const uint albedo_texture = spec.color_metallic_roughness_textures & 0xFFFF;

    float alpha = 1.0f;
    if(HAS_MATERIAL(ALBEDO))
    {
        alpha = unpackUnorm4x8(spec.color).a;
        if(albedo_texture != UNUSED_TEXTURE)
            alpha *= textureGrad(textures[nonuniformEXT(albedo_texture)], uv, puvdx, puvdy).a;
    }

    return alpha;
}

float sample_material_alpha(
    material_spec spec, vec2 uv
#ifndef IMPLICIT_GRADIENT_SAMPLING
    , vec2 puvdx, vec2 puvdy
#endif
){
    return sample_material_alpha_core(spec, uv, SAMPLE_TEXTURE_GRADIENT(uv));
}

vec3 sample_cubemap(int ind, vec3 dir, float lod)
{
    return textureLod(cube_textures[nonuniformEXT(ind)], vec3(dir.x, dir.y, -dir.z), lod).rgb;
}

material sample_material_and_decal(
    material_spec spec, bool front_facing, in vertex_data vd
#ifndef IMPLICIT_GRADIENT_SAMPLING
    , vec2 puvdx, vec2 puvdy
#endif
){
    material mat = sample_material_core(spec, front_facing, vd.uv.xy, SAMPLE_TEXTURE_GRADIENT(vd.uv.xy));

    FOR_DECALS(d, vd.pos)
        vec3 local = (vec4(vd.pos, 1) * d.world_to_obb).xyz;
        float angle = dot(vd.tangent_space[2], normalize(d.world_to_obb[2].xyz));
        if(any(greaterThan(abs(local.xyz), vec3(1))) || angle < 0.2)
            continue;

        float z_fade = 1.0f - smoothstep(0.8f, 1.0f, abs(local.z));

        local = local * 0.5f + 0.5f;

        material decal_material = sample_material_core(d.material, front_facing, local.xy, SAMPLE_TEXTURE_GRADIENT(local.xy));
        decal_material.albedo.a *= smoothstep(0.2, 0.3, angle) * z_fade;
        mat = blend_material(mat, decal_material);
    END_DECALS

    return mat;
}

// Returns the resulting tangent space
mat3 apply_normal_map(mat3 tangent_space, inout material mat)
{
    vec3 normal = normalize(tangent_space * mat.normal);
    mat3 normal_tangent_space = tangent_space;
    normal_tangent_space[2] = normal;
    normal_tangent_space = orthogonalize(normal_tangent_space);
    mat.normal = vec3(0,0,1);
    if(HAS_MATERIAL(ANISOTROPY))
    {
        float sr = sin(mat.anisotropy_rotation);
        float cr = cos(mat.anisotropy_rotation);
        normal_tangent_space = mat3(
            cr * normal_tangent_space[0] - sr * normal_tangent_space[1],
            sr * normal_tangent_space[0] + cr * normal_tangent_space[1],
            normal_tangent_space[2]
        );
    }
    if(HAS_MATERIAL(CLEARCOAT))
    {
        vec3 clearcoat_normal = normalize(tangent_space * mat.clearcoat_normal);
        // We move the world-space clearcoat normal into the new normal tangent
        // space.
        mat.clearcoat_normal = clearcoat_normal * normal_tangent_space;
    }
    return normal_tangent_space;
}

struct env_state
{
    ivec4 indices;
    mat3 world_to_envmap;
    vec3 envmap_pos;
    vec3 ambient;
};

vec3 sample_environment_radiance(
    vec3 dir,
    float roughness,
    env_state env
){
    if(HAS_MATERIAL(ENVMAP_SPECULAR) && env.indices.x != -1)
    { // Envmap
        float lod = sqrt(roughness) * float(textureQueryLevels(cube_textures[nonuniformEXT(env.indices.x)])-2);
        vec3 envmap_dir = env.world_to_envmap * dir;
        if(HAS_MATERIAL(ENVMAP_PARALLAX) && env.indices.y != 0)
        { // Parallax mapping
            if(env.indices.y == 1)
                envmap_dir = box_parallax(env.envmap_pos, envmap_dir);
            else
                envmap_dir = sphere_parallax(env.envmap_pos, envmap_dir);
        }
        return textureLod(
            cube_textures[nonuniformEXT(env.indices.x)],
            envmap_dir,
            lod
        ).rgb;
    }
    else
    { // Ambient fallback
        return env.ambient;
    }
}

vec3 sample_environment_irradiance(
    vec3 pos,
    vec3 normal,
    bool transmission,
    vec2 lightmap_uv,
    env_state env
){
    if(HAS_MATERIAL(LIGHTMAP) && env.indices.z >= 0 && !transmission)
    { // Prefer lightmap where available
        return textureLod(textures[nonuniformEXT(env.indices.z)], lightmap_uv, 0).rgb;
    }
    else return env.ambient;
}

vec3 get_indirect_light(
    vec3 pos,
    ivec4 environment,
    vec3 ambient_fallback,
    mat3 tangent_space,
    vec3 smooth_normal,
    vec3 view, // in tangent space
    in material mat,
    vec2 lightmap_uv
){
    vec3 indirect = vec3(0);
    vec3 attenuation = vec3(1.0f);

    env_state env;
    env.indices = environment;
    env.ambient = ambient_fallback;

    if(HAS_MATERIAL(ENVMAP_SPECULAR) && environment.x != -1)
    {
        mat4 world_to_envmap = envmap_metadata.world_to_envmap[environment.x];
        env.world_to_envmap = mat3(world_to_envmap);
        if(HAS_MATERIAL(ENVMAP_PARALLAX))
            env.envmap_pos = (world_to_envmap * vec4(pos, 1)).xyz;
    }

    if(HAS_MATERIAL(CLEARCOAT) && mat.clearcoat != 0)
    {
        float v_dot_n = max(dot(view, mat.clearcoat_normal), 0.0f);
        vec3 reflected;
        float local_attenuation = clearcoat_brdf_indirect(
            mat,
            texture(material_lut, vec2(v_dot_n, sqrt(mat.clearcoat_roughness))).xy,
            v_dot_n,
            reflected
        );
        vec3 ref_dir = -tangent_space * clamped_reflect(view, mat.clearcoat_normal);
        indirect += attenuation * reflected * sample_environment_radiance(
            ref_dir, mat.clearcoat_roughness, env);
        attenuation *= local_attenuation;
    }

    if(HAS_MATERIAL(METALLIC_ROUGHNESS))
    {
        float v_dot_n = max(view.z, 0.0f);
        vec3 reflected;
        vec3 refracted;
        float local_attenuation = trowbridge_reitz_bsdf_indirect(
            mat,
            texture(material_lut, vec2(v_dot_n, sqrt(mat.roughness))).xy,
            v_dot_n,
            reflected,
            refracted
        );
        vec3 reflect_dir = -tangent_space * clamped_reflect(
            view, vec3(0,0,1)
        );
        indirect += attenuation * reflected * sample_environment_radiance(
            reflect_dir, mat.roughness, env);
        if(HAS_MATERIAL(TRANSMISSION) && any(greaterThan(refracted, vec3(1e-7))))
        {
            vec3 refract_dir = tangent_space * refract(
                -view, vec3(0,0,1), mat.eta
            );
            indirect += attenuation * refracted * sample_environment_radiance(
                refract_dir, mat.roughness, env);
        }
        attenuation *= local_attenuation;
    }

    if(mat.metallic < 1.0f)
    {
        attenuation *= mat.albedo.rgb * (1.0f - mat.metallic);
        indirect += sample_environment_irradiance(pos, tangent_space[2], false, lightmap_uv, env) *
            attenuation * (1.0f - mat.transmission);
        if(HAS_MATERIAL(TRANSMISSION) && mat.translucency > 0)
        {
            indirect += sample_environment_irradiance(pos, -tangent_space[2], true, lightmap_uv, env) *
                attenuation * mat.transmission * mat.translucency;
        }
    }

    return indirect;
}

#endif
