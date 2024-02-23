#ifndef RAYBASE_GFX_PATH_TRACER_CORE_GLSL
#define RAYBASE_GFX_PATH_TRACER_CORE_GLSL

#ifndef RAYBASE_GFX_PATH_TRACER_GLSL
#error "You must include path_tracer.glsl before including this file, and define your buffers containing path_tracer_config in between."
#endif

#ifndef RB_PT
#define RB_PT pc.config
#endif

#include "alias_table.glsl"

layout(binding = 0, set = 2) readonly buffer envmap_alias_table_buffer
{
    alias_table_entry entries[];
} envmap_alias_table;

#include "envmap.glsl"

layout(location = 0) rayPayloadEXT bounce_payload payload;

// This variant is used for weighting the BSDF sampling.
float get_mis_pdf(
    int point_light_count,
    intersection_info info,
    float bsdf_pdf
){
    if(bsdf_pdf == 0.0f) return 1.0f;

    float nee_pdf2 = 0.0f;
    if((RB_PATH_TRACING_FLAGS & NEE_SAMPLE_ALL_LIGHTS) != 0)
    {
        nee_pdf2 =
            info.directional_light_pdf * info.directional_light_pdf +
            info.point_light_pdf * info.point_light_pdf +
            info.tri_light_pdf * info.tri_light_pdf +
            info.envmap_pdf * info.envmap_pdf;
    }
    else
    {
        vec4 prob = RB_PT.light_type_sampling_probabilities;

        nee_pdf2 =
            info.point_light_pdf / max(point_light_count, 1) * prob.x +
            info.directional_light_pdf * prob.y +
            info.tri_light_pdf * prob.z +
            info.envmap_pdf * prob.w;
        nee_pdf2 *= nee_pdf2;
    }
    return (bsdf_pdf * bsdf_pdf + nee_pdf2) / bsdf_pdf;
}

// This variant is used for weighting the BSDF sampling, when no other lights
// than the envmap are directly sampled.
float get_envmap_mis_pdf(intersection_info info, float bsdf_pdf)
{
    if(bsdf_pdf == 0.0f) return 1.0f;

    float nee_pdf = info.envmap_pdf;
    return (bsdf_pdf * bsdf_pdf + nee_pdf * nee_pdf) / bsdf_pdf;
}

// This variant is used for weighting lights for the NEE sampling.
float get_mis_pdf(int sampled_light_count, float nee_pdf, float bsdf_pdf)
{
    if(nee_pdf == 0.0f) return 1.0f;

    return (nee_pdf * nee_pdf + bsdf_pdf * bsdf_pdf) / nee_pdf;
}

bool get_intersection_info(
    bool primary_ray,
    vec3 ray_origin,
    vec3 ray_direction,
    bounce_payload payload,
    out intersection_info info
){
    info.light = vec3(0);

    if(payload.instance_index >= 0)
    { // Mesh
        instance i = instances.array[nonuniformEXT(payload.instance_index)];
        info.vd = get_vertex_data(
            payload.instance_index,
            payload.primitive_index,
            payload.hit_attribs,
            (i.flags & INSTANCE_FLAG_HAS_TRI_LIGHTS) != 0
        );

        bool front_facing = dot(ray_direction, info.vd.flat_normal) < 0;
        const bool allow_decals = RB_DECAL_MODE >= (primary_ray ? TRACE_DECALS_PRIMARY : TRACE_DECALS_ALWAYS);
        if(allow_decals && (i.flags & INSTANCE_FLAG_RECEIVES_DECALS) != 0)
            info.mat = sample_material_and_decal(i.material, front_facing, info.vd, vec2(0.0), vec2(0.0f));
        else
            info.mat = sample_material(i.material, front_facing, info.vd.uv.xy, vec2(0.0), vec2(0.0f));

        info.vd.tangent_space = apply_normal_map(info.vd.tangent_space, info.mat);
        info.point_light_pdf = 0.0f;
        info.directional_light_pdf = 0.0f;
        info.tri_light_pdf = (i.flags & INSTANCE_FLAG_HAS_TRI_LIGHTS) != 0 ?
            1.0f / (triangle_solid_angle(
                ray_origin,
                info.vd.triangle_pos[0],
                info.vd.triangle_pos[1],
                info.vd.triangle_pos[2]
            ) * scene_params.tri_light_count) : 0.0f;
        info.envmap_pdf = 0.0f;

        return true;
    }
    else if(payload.primitive_index >= 0)
    { // Point light
        point_light pl = point_lights.array[payload.primitive_index];

        vec2 radius_and_cutoff_radius = unpackHalf2x16(pl.radius_and_cutoff_radius);
        // No worries, radius cannot be zero -- we couldn't hit the light here
        // if it was!
        float radius = radius_and_cutoff_radius.x;
        float cutoff_radius = radius_and_cutoff_radius.y;

        vec3 pos = vec3(pl.pos_x, pl.pos_y, pl.pos_z);
        float dist = distance(ray_origin, pos);

        info.light = (primary_ray ? 1.0f : get_light_range_cutoff(cutoff_radius, dist)) * rgbe_to_rgb(pl.color) / (M_PI * radius * radius);
        info.point_light_pdf = dist < cutoff_radius ? sample_point_light_pdf(pl, ray_origin) : 0.0f;
        info.directional_light_pdf = 0.0f;
        info.tri_light_pdf = 0.0f;
        info.envmap_pdf = 0.0f;
        return false;
    }
    else
    { // Miss
        vec3 sample_dir = quat_rotate(scene_params.envmap_orientation, ray_direction);
        info.light = scene_params.envmap_index >= 0 ?
            textureLod(cube_textures[scene_params.envmap_index], sample_dir, 0.0f).rgb :
            vec3(0);

        info.point_light_pdf = 0.0f;
        info.directional_light_pdf = 0.0f;
        info.tri_light_pdf = 0.0f;
        info.envmap_pdf = scene_params.envmap_index >= 0 ? sample_environment_map_pdf(sample_dir) : 0.0f;

        int directional_light_count = 0;
        FOR_DIRECTIONAL_LIGHTS(light)
            vec3 dir;
            vec3 color;
            get_directional_light_info(light, dir, color);
            float visible = step(1-dot(ray_direction, dir), light.solid_angle);
            float pdf = sample_directional_light_pdf(light);
            info.light += visible * color * (pdf == 0.0f ? 1.0f : pdf);
            info.directional_light_pdf += visible * pdf;
            directional_light_count += int(visible);
        END_DIRECTIONAL_LIGHTS
        info.directional_light_pdf /= max(directional_light_count, 1);
        return false;
    }
}

vec2 sample_film(ivec2 p, uvec2 size, vec2 u)
{
    // Film sampling
    vec2 film_offset = vec2(0);
    if(RB_FILM_SAMPLING_MODE == FILM_FILTER_BOX)
        film_offset = (u - 0.5f) * RB_PT.film_parameter;
    else if(RB_FILM_SAMPLING_MODE == FILM_FILTER_GAUSSIAN)
        film_offset = sample_gaussian_weighted_disk(u, RB_PT.film_parameter);
    else if(RB_FILM_SAMPLING_MODE == FILM_FILTER_BLACKMAN_HARRIS)
        film_offset = sample_blackman_harris_weighted_disk(u) * RB_PT.film_parameter;
    return (vec2(p) + 0.5f + film_offset) / vec2(size);
}

void get_camera_ray(uint camera_index, ivec2 p, inout uvec4 seed, out vec3 origin, out vec3 dir)
{
    camera cam = cameras.array[camera_index];
    vec4 u = generate_uniform_random(seed);

    // Film sampling
    vec2 uv = sample_film(p, gl_LaunchSizeEXT.xy, u.xy);

    // Depth-of-field
    vec2 aperture_pos;
    if(RB_APERTURE_SHAPE == 0)
        aperture_pos = vec2(0);
    else if(RB_APERTURE_SHAPE == 1)
        aperture_pos = sample_disk(u.zw);
    else
        aperture_pos = sample_regular_polygon(u.zw, cam.aperture_angle, RB_APERTURE_SHAPE);

    // Calculate ray!
    if(RB_APERTURE_SHAPE == 0)
        get_pinhole_camera_ray(uv, cam, origin, dir);
    else
        get_thin_lens_camera_ray(uv, cam, aperture_pos, origin, dir);
}

// Returns false on ray termination.
bool trace_ray(
    inout uvec4 seed,
    out intersection_info info,
    vec3 origin,
    vec3 dir
){
    payload.seed = seed.x;
    traceRayEXT(
        scene_tlas,
        (RB_PATH_TRACING_FLAGS & STOCHASTIC_ALPHA_BLENDING) == 0 ? gl_RayFlagsOpaqueEXT : gl_RayFlagsNoneEXT,
        RB_INDIRECT_RAY_MASK,
        0,
        0,
        0,
        origin,
        RB_PT.min_ray_dist,
        dir,
        RB_PT.max_ray_dist,
        0
    );
    seed.x = payload.seed;

    return get_intersection_info(false, origin, dir, payload, info);
}

// The difference to trace_ray is small, this just handles some flags differently.
bool trace_primary_ray(inout uvec4 seed, out intersection_info info, vec3 origin, vec3 dir)
{
    payload.seed = seed.x;
    traceRayEXT(
        scene_tlas,
        ((RB_PATH_TRACING_FLAGS & CULL_BACK_FACES) != 0 ? gl_RayFlagsCullBackFacingTrianglesEXT : ((RB_PATH_TRACING_FLAGS & STOCHASTIC_ALPHA_BLENDING) == 0 ? gl_RayFlagsOpaqueEXT : gl_RayFlagsNoneEXT)),
        RB_DIRECT_RAY_MASK,
        0,
        0,
        0,
        origin,
        0.0f,
        dir,
        RB_PT.max_ray_dist,
        0
    );
    seed.x = payload.seed;

    return get_intersection_info(true, origin, dir, payload, info);
}

float trace_shadow_ray(inout uint seed, vec3 origin, vec3 dir, float len)
{
    payload.seed = seed.x;
    traceRayEXT(
        scene_tlas,
        ((RB_PATH_TRACING_FLAGS & CULL_BACK_FACES) != 0 ? gl_RayFlagsCullBackFacingTrianglesEXT : ((RB_PATH_TRACING_FLAGS & STOCHASTIC_ALPHA_BLENDING) == 0 ? gl_RayFlagsOpaqueEXT : gl_RayFlagsNoneEXT)) | gl_RayFlagsTerminateOnFirstHitEXT,
        RB_INDIRECT_RAY_MASK&(~TLAS_LIGHT_MASK),
        0, 0, 0,
        origin,
        RB_PT.min_ray_dist,
        dir,
        len,
        0
    );
    seed.x = payload.seed;
    return payload.instance_index >= 0 ? 0.0f : 1.0f;
}

bool self_shadow(vec3 view, vec3 flat_normal, vec3 light_dir, material mat)
{
    return dot(flat_normal, light_dir) * dot(view, flat_normal) < 0 && (!HAS_MATERIAL(TRANSMISSION) || mat.transmission == 0.0f);
}

vec3 eval_explicit_light(
    inout uint seed,
    vec3 light_color,
    vec3 dir,
    float len,
    vec3 tview,
    int sampled_light_count,
    float nee_pdf,
    material mat,
    vertex_data vd
){
    vec3 tdir = dir * vd.tangent_space;
    vec3 reflected, transmitted;
    float bsdf_pdf = full_bsdf_pdf(mat, tdir, tview, reflected, transmitted);
    vec3 color = light_color * (mat.albedo.rgb * transmitted + reflected);
    color = self_shadow(tview, vd.flat_normal * vd.tangent_space, tdir, mat) ? vec3(0) : color;
    color *= trace_shadow_ray(seed, vd.pos, dir, len);

    if(nee_pdf == 0.0f) return color;
    else if(nee_pdf < 0) return vec3(0);

    return color / get_mis_pdf(sampled_light_count, nee_pdf, bsdf_pdf);
}

vec3 eval_all_explicit_lights(
    inout uvec4 seed,
    out int point_light_count,
    vec3 tview,
    material mat,
    vertex_data vd
){
    vec3 color = vec3(0);

    point_light_count = get_point_light_count_in_range(vd.pos);

    FOR_POINT_LIGHTS(light, vd.pos)
        float cutoff_radius = unpackHalf2x16(light.radius_and_cutoff_radius).g;
        vec3 delta = vec3(light.pos_x, light.pos_y, light.pos_z) - vd.pos;
        if(cutoff_radius * cutoff_radius > dot(delta, delta))
        {
            vec4 u = generate_uniform_random(seed);
            vec3 dir = vec3(0);
            float len = 0.0f;
            vec3 col;
            float nee_pdf = 0.0f;
            sample_point_light(light, u.xy, vd.pos, dir, len, col, nee_pdf);
            if(nee_pdf == 0.0f) col /= len * len;
            color += eval_explicit_light(seed.x, col, dir, len, tview, point_light_count, nee_pdf, mat, vd);
        }
    END_POINT_LIGHTS

    FOR_DIRECTIONAL_LIGHTS(light)
        vec4 u = generate_uniform_random(seed);
        vec3 dir = vec3(0);
        float len = RB_PT.max_ray_dist;
        vec3 col;
        float nee_pdf = 0.0f;
        sample_directional_light(light, u.xy, dir, col, nee_pdf);
        color += eval_explicit_light(seed.x, col, dir, len, tview, int(scene_params.directional_light_count), nee_pdf, mat, vd);
    END_DIRECTIONAL_LIGHTS

    if(scene_params.envmap_index >= 0)
    {
        vec3 dir = vec3(0);
        float len = RB_PT.max_ray_dist;
        float nee_pdf = 0.0f;
        pcg4d(seed);
        vec3 col = sample_environment_map(seed, dir, nee_pdf);
        color += eval_explicit_light(seed.x, col, dir, len, tview, 1, nee_pdf, mat, vd);
    }

    return color;
}

vec3 sample_any_explicit_light(
    inout uvec4 seed,
    vec3 pos,
    out vec3 sample_dir,
    out float sample_dir_len,
    out float nee_pdf
){
    vec4 prob = RB_PT.light_type_sampling_probabilities;
    vec4 u = generate_uniform_random(seed);
    vec3 sample_color = vec3(0);
    sample_dir = vec3(0);
    sample_dir_len = 0.0f;
    nee_pdf = 0.0f;

    float local_pdf = 0.0f;
    if((u.x -= prob.x) < 0)
    { // Point light
        point_light pl;
        int light_count = pick_random_point_light_in_range(pos, pl, seed.x);
        sample_point_light(pl, u.yz, pos, sample_dir, sample_dir_len, sample_color, nee_pdf);

        if(nee_pdf == 0.0f) sample_color /= sample_dir_len * sample_dir_len;

        local_pdf = prob.x / max(light_count, 1);
    }
    else if((u.x -= prob.y) < 0)
    { // Directional light
        int i = clamp(
            int(u.y * scene_params.directional_light_count),
            0, int(scene_params.directional_light_count)-1
        );
        directional_light dl = directional_lights.array[i];
        sample_directional_light(dl, u.zw, sample_dir, sample_color, nee_pdf);
        sample_dir_len = RB_PT.max_ray_dist;
        local_pdf = prob.y / scene_params.directional_light_count;
    }
    else if((u.x -= prob.z) < 0)
    { // Tri light
        int i = clamp(
            int(u.y * scene_params.tri_light_count),
            0, int(scene_params.tri_light_count)-1
        );
        tri_light tl = tri_lights.array[i];
        sample_tri_light(tl, u.zw, pos, sample_dir, sample_dir_len, sample_color, nee_pdf);
        sample_dir_len -= RB_PT.min_ray_dist;
        if(isinf(nee_pdf) || nee_pdf <= 0 || any(isnan(sample_dir)))
            nee_pdf = -1.0f;
        local_pdf = prob.z / scene_params.tri_light_count;
    }
    else
    { // Envmap
        sample_dir_len = RB_PT.max_ray_dist;
        pcg4d(seed);
        sample_color = sample_environment_map(seed, sample_dir, nee_pdf);
        local_pdf = prob.w;
    }

    if(nee_pdf == 0) sample_color /= local_pdf;
    else nee_pdf *= local_pdf;

    return sample_color;
}

vec3 eval_any_explicit_light(inout uvec4 seed, vec3 tview, material mat, vertex_data vd)
{
    vec3 dir;
    float len;
    float pdf;
    vec3 color = sample_any_explicit_light(seed, vd.pos, dir, len, pdf);
    return eval_explicit_light(seed.x, color, dir, len, tview, 1, pdf, mat, vd);
}

vec3 eval_envmap_only(
    inout uvec4 seed,
    vec3 tview,
    material mat,
    vertex_data vd
){
    pcg4d(seed);

    if(scene_params.envmap_index >= 0)
    {
        vec3 dir = vec3(0);
        float len = RB_PT.max_ray_dist;
        float nee_pdf = 0.0f;
        pcg4d(seed);
        vec3 color = sample_environment_map(seed, dir, nee_pdf);
        return eval_explicit_light(seed.x, color, dir, len, tview, 1, nee_pdf, mat, vd);
    }
    else return vec3(0);
}

vec3 clamp_contribution(vec3 contrib)
{
    if((RB_PATH_TRACING_FLAGS & USE_CLAMPING) != 0)
    {
        float m = luminance(contrib);
        if(m > RB_PT.clamping_threshold)
            contrib = contrib * RB_PT.clamping_threshold / m;
    }
    return contrib;
}

vec3 trace_indirect_path(
    inout uvec4 seed,
    inout intersection_info info,
    vec3 origin,
    vec3 tview
){
    vec3 output_color = vec3(0);
    vec3 attenuation = vec3(1);
    float regularization = 1.0f;

    [[unroll]] for(int bounce = 0; bounce < RB_MAX_BOUNCES; ++bounce)
    {
        float original_roughness = info.mat.roughness;
        // Regularization strategy inspired by "Optimised Path Space Regularisation", 2021 Weier et al.
        if((RB_PATH_TRACING_FLAGS & USE_PATH_SPACE_REGULARIZATION) != 0)
        {
            if(bounce != 0)
            {
                info.mat.roughness = 1 - ((1 - info.mat.roughness) * regularization);
                if(HAS_MATERIAL(CLEARCOAT))
                    info.mat.clearcoat_roughness = 1 - ((1 - info.mat.clearcoat_roughness) * regularization);
            }
            regularization *= max(1 - RB_PT.regularization_gamma * original_roughness, 0.0f);
        }

        int point_light_count = 1;
        vec3 nee_color = vec3(0);
        if((RB_PATH_TRACING_FLAGS & NEE_SAMPLE_ALL_LIGHTS) != 0)
            nee_color = eval_all_explicit_lights(seed, point_light_count, tview, info.mat, info.vd);
        else
            nee_color = eval_any_explicit_light(seed, tview, info.mat, info.vd);

        nee_color *= attenuation;
        if(bounce != 0) nee_color = clamp_contribution(nee_color);
        output_color += nee_color;

        if((RB_PATH_TRACING_FLAGS & PATH_SPACE_REGULARIZATION_NEE_ONLY) != 0)
            info.mat.roughness = original_roughness;

        vec3 tdir = vec3(0,0,1);
        float bsdf_pdf = 0.0f;
        attenuation *= full_bsdf_sample(generate_uniform_random(seed).xyz, info.mat, tview, tdir, bsdf_pdf);

        vec3 view = info.vd.tangent_space * tdir;

        bool shadowed = self_shadow(tview, info.vd.flat_normal * info.vd.tangent_space, tdir, info.mat);
        if(shadowed || all(lessThan(attenuation, vec3(1e-9))))
            break;

        bool terminate = !trace_ray(seed, info, info.vd.pos, view);

        vec3 local_attenuation = attenuation / get_mis_pdf(point_light_count, info, bsdf_pdf);
        attenuation /= (bsdf_pdf == 0 ? 1.0f : bsdf_pdf);

        tview = (-view) * info.vd.tangent_space;
        if(tview.z < 1e-7f)
            tview = vec3(tview.xy, max(tview.z, 1e-7f));
        tview = normalize(tview);

        if(terminate)
        {
            output_color += clamp_contribution(local_attenuation * info.light);
            break;
        }
        else output_color += clamp_contribution(local_attenuation * info.mat.emission);
    }
    return output_color;
}

vec3 trace_path(inout uvec4 seed, vec3 origin, vec3 view)
{
    intersection_info info;
    vec3 output_color = vec3(0);
    if(trace_primary_ray(seed, info, origin, view))
    {
        output_color += info.mat.emission;

        // Direct lighting
        vec3 tview = (-view) * info.vd.tangent_space;
        if(tview.z < 1e-7f)
            tview = vec3(tview.xy, max(tview.z, 1e-7f));
        tview = normalize(tview);

        output_color += trace_indirect_path(seed, info, origin, tview);
    }
    else output_color += info.light;
    return output_color;
}

#endif
