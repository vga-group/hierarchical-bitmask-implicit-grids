#ifndef RAYBASE_GFX_FORWARD_GLSL
#define RAYBASE_GFX_FORWARD_GLSL

#define RAYBASE_SCENE_SET 0
#define IMPLICIT_GRADIENT_SAMPLING
#include "scene.glsl"

struct rasterizer_config
{
    vec4 ambient;
    float directional_min_radius;
    float point_min_radius;
    float directional_pcf_bias;
    float point_pcf_bias;
};

layout(constant_id = 5) const uint RB_DIRECTIONAL_SHADOW_FILTERING_TYPE = 0;
layout(constant_id = 6) const uint RB_DIRECTIONAL_PCF_SAMPLES = 16;
layout(constant_id = 7) const uint RB_DIRECTIONAL_PCSS_SAMPLES = 8;
layout(constant_id = 8) const uint RB_POINT_SHADOW_FILTERING_TYPE = 0;
layout(constant_id = 9) const uint RB_POINT_PCF_SAMPLES = 16;
layout(constant_id = 10) const uint RB_POINT_PCSS_SAMPLES = 16;
layout(constant_id = 11) const uint RB_USE_DYNAMIC_LIGHTING = 1;
layout(constant_id = 12) const uint RB_ALPHA_DISCARD = 0;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_tangent;
layout(location = 3) in vec3 in_bitangent;
layout(location = 4) in vec2 in_uv;
layout(location = 5) in vec2 in_lightmap_uv;

void get_surface_info(
    instance inst,
    out vertex_data vd,
    out material mat
){
    vd.pos = in_pos;
    vd.uv = vec4(in_uv, in_lightmap_uv);
    vd.flat_normal = normalize(cross(dFdy(in_pos), dFdx(in_pos)));
    vd.smooth_normal = in_normal;
    vd.tangent_space = mat3(
        normalize(in_tangent),
        normalize(in_bitangent),
        normalize(in_normal)
    );

    if((inst.flags & INSTANCE_FLAG_RECEIVES_DECALS) != 0)
        mat = sample_material_and_decal(inst.material, gl_FrontFacing, vd);
    else
        mat = sample_material(inst.material, gl_FrontFacing, vd.uv.xy);

    vd.tangent_space = apply_normal_map(vd.tangent_space, mat);
}

vec4 eval_light_contribution(
    instance inst,
    vec3 view,
    vertex_data vd,
    material mat,
    rasterizer_config pc
){
    vec3 normal = vd.tangent_space[2];
    vec3 tview = view * vd.tangent_space;
    // View vector must never be from below the surface, that makes no sense
    // (due to our normal flipping, this is nonsensical, but caused by
    // normal mapping)
    tview = normalize(vec3(tview.xy, max(tview.z, 0.00001f)));

    vec3 contrib = mat.emission;
    contrib += get_indirect_light(
        vd.pos, inst.environment, pc.ambient.rgb,
        vd.tangent_space, vd.smooth_normal, tview, mat, vd.uv.zw
    );

    if(RB_USE_DYNAMIC_LIGHTING == 1)
    {
        FOR_POINT_LIGHTS(light, vd.pos)
            vec3 dir;
            vec3 color;
            if(get_point_light_info(light, vd.pos, dir, color))
            {
                vec3 tdir = dir * vd.tangent_space;
                vec3 reflected, transmitted;
                full_bsdf(mat, tdir, tview, reflected, transmitted);
                contrib += color * (mat.albedo.rgb * transmitted + reflected);
            }
        END_POINT_LIGHTS

        FOR_DIRECTIONAL_LIGHTS(light)
            vec3 dir;
            vec3 color;
            get_directional_light_info(light, dir, color);
            color *= light.solid_angle == 0 ? 1.0f : 2.0f * M_PI * light.solid_angle;
            vec3 tdir = dir * vd.tangent_space;
            vec3 reflected, transmitted;
            full_bsdf(mat, tdir, tview, reflected, transmitted);
            contrib += color * (mat.albedo.rgb * transmitted + reflected);
        END_DIRECTIONAL_LIGHTS
    }

    return vec4(contrib, mat.albedo.a);
}

#endif
