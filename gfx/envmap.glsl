#ifndef RAYBASE_GFX_ENVMAP_GLSL
#define RAYBASE_GFX_ENVMAP_GLSL
#include "math.glsl"

vec3 sample_environment_map(
    uvec4 rand32,
    out vec3 shadow_ray_direction,
    out float pdf
){
    const uint face_size = scene_params.envmap_face_size_x * scene_params.envmap_face_size_y;
    // Assuming envmap_face_size is a power of two or fairly small, this should
    // be okay-ish.
    uint i = (rand32.x % face_size) + face_size * (rand32.y % 6);
    alias_table_entry at = envmap_alias_table.entries[i];
    pdf = at.pdf;
    if(rand32.z > at.probability)
    {
        i = at.alias_id;
        pdf = at.alias_pdf;
    }

    vec3 dir = pixel_id_to_cubemap_direction(
        int(i), ldexp(rand32.zw, ivec2(-32)),
        ivec2(scene_params.envmap_face_size_x, scene_params.envmap_face_size_y)
    );
    vec3 sample_dir = quat_rotate(quat_inverse(scene_params.envmap_orientation), dir);
    shadow_ray_direction = normalize(sample_dir);
    pdf /= cubemap_jacobian_determinant(dir);

    return textureLod(cube_textures[scene_params.envmap_index], dir, 0.0f).rgb;
}

float sample_environment_map_pdf(vec3 dir)
{
    const uint face_size = scene_params.envmap_face_size_x * scene_params.envmap_face_size_y;
    uint i = cubemap_direction_to_pixel_id(dir, ivec2(scene_params.envmap_face_size_y, scene_params.envmap_face_size_y));
    alias_table_entry at = envmap_alias_table.entries[i];
    return at.pdf / cubemap_jacobian_determinant(dir);
}

#endif
