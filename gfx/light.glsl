#ifndef RAYBASE_GFX_LIGHT_GLSL
#define RAYBASE_GFX_LIGHT_GLSL
#include "math.glsl"
#include "clustering.glsl"
#include "color.glsl"
#include "material_data.glsl"

struct point_light
{
    float pos_x;
    float pos_y;
    float pos_z;
    uint color; // RGBE
    uint radius_and_cutoff_radius; // Packed halfs
    uint cutoff_angle_and_directional_falloff_exponent; // Packed halfs
    uint direction; // Two packed halfs, octahedral encoding
    uint shadow_map_index_and_spot_radius; // uint16_t and half
};

struct directional_light
{
    uint color; // RGBE
    uint direction; // Two packed halfs, octahedral encoding
    float solid_angle;
    int shadow_map_index;
};

struct tri_light
{
    vec4 corners[3]; // Corner positions in RGB, floatBitsToUint & unpackHalf2x16 UV coordinate in A
    uint emission_factor; // RGBE
    uint emission_tex; // Could be compressed to 16 bits in the future.
    // These map the triangle light to its original instance.
    uint instance_id;
    uint triangle_id;
};

float get_light_range_cutoff(float cutoff_radius, float dist)
{
    return 1.0f-smoothstep(cutoff_radius*0.90, cutoff_radius, dist);
}

bool get_spotlight_visible(point_light l, vec3 dir)
{
    if(l.cutoff_angle_and_directional_falloff_exponent != 0)
    {
        float cutoff_angle = unpackHalf2x16(l.cutoff_angle_and_directional_falloff_exponent).x;
        vec3 spot_dir = octahedral_decode(unpackSnorm2x16(l.direction));
        float cutoff = 1.0f - dot(dir, -spot_dir);
        return cutoff < cutoff_angle;
    }
    return true;
}

float get_spotlight_cutoff(point_light l, vec3 dir)
{
    if(l.cutoff_angle_and_directional_falloff_exponent != 0)
    {
        vec2 cutoff_angle_and_directional_falloff_exponent =
            unpackHalf2x16(l.cutoff_angle_and_directional_falloff_exponent);

        float cutoff_angle = cutoff_angle_and_directional_falloff_exponent.x;
        float directional_falloff_exponent = cutoff_angle_and_directional_falloff_exponent.y;

        vec3 spot_dir = octahedral_decode(unpackSnorm2x16(l.direction));
        float cutoff = 1.0f - dot(dir, -spot_dir);
        return cutoff < cutoff_angle ? 1.0f - pow(max(cutoff, 0.0f)/cutoff_angle, directional_falloff_exponent) : 0.0f;
    }
    return 1.0f;
}

// RASTER VERSION! Don't use this in a path tracer, it makes assumptions about
// your sampling when it scales the color brightness!
bool get_point_light_info(
    point_light l,
    vec3 pos,
    out vec3 dir,
    out vec3 color
){
    dir = vec3(l.pos_x, l.pos_y, l.pos_z) - pos;
    float dist2 = dot(dir, dir);
    float dist = sqrt(dist2);
    dir /= dist;

    vec2 rc = unpackHalf2x16(l.radius_and_cutoff_radius).xy;

    color = get_light_range_cutoff(rc.y, dist) * rgbe_to_rgb(l.color) / dist2;

    color *= get_spotlight_cutoff(l, dir);
    return dist < rc.y;
}

void get_directional_light_info(
    directional_light l,
    out vec3 dir,
    out vec3 color
){
    dir = -octahedral_decode(unpackSnorm2x16(l.direction));
    color = rgbe_to_rgb(l.color);
}

int get_shadow_map_index(point_light l)
{
    int shadow_map_index = int(l.shadow_map_index_and_spot_radius & 0xFFFF);
    if(shadow_map_index == 0xFFFF) return -1;
    return shadow_map_index;
}

int get_shadow_map_index(directional_light l)
{
    return l.shadow_map_index;
}

void sample_point_light(
    point_light l,
    vec2 u,
    vec3 pos,
    out vec3 dir,
    out float dist,
    out vec3 color,
    out float pdf
){
    vec3 center_dir = vec3(l.pos_x, l.pos_y, l.pos_z) - pos;
    float dist2 = dot(center_dir, center_dir);

    vec2 radius_and_cutoff_radius = unpackHalf2x16(l.radius_and_cutoff_radius);
    float radius = radius_and_cutoff_radius.x;
    float cutoff_radius = radius_and_cutoff_radius.y;

    float cos2_theta = 1.0f - radius * radius / dist2;
    float cos_theta = cos2_theta > 0 ? sqrt(cos2_theta) : -1.0f;

    dir = sample_cone(normalize(center_dir), cos_theta, u.xy);
    float cd = dot(center_dir, dir);
    dist = cd - sqrt(max(cd * cd + radius * radius - dist2, 0.0f));

    color = get_light_range_cutoff(cutoff_radius, sqrt(dist2)) *
        rgbe_to_rgb(l.color) * get_spotlight_cutoff(l, normalize(center_dir));

    if(radius == 0.0f)
    {
        pdf = 0;
    }
    else
    {
        color /= M_PI * radius * radius;
        pdf = 1 / (2.0f * M_PI * (1.0f - cos_theta));
    }
}

float sample_point_light_pdf(point_light l, vec3 pos)
{
    vec3 center_dir = vec3(l.pos_x, l.pos_y, l.pos_z) - pos;
    float dist2 = dot(center_dir, center_dir);

    float radius = unpackHalf2x16(l.radius_and_cutoff_radius).x;

    float cos2_theta = 1.0f - radius * radius / dist2;
    float cos_theta = cos2_theta > 0 ? sqrt(cos2_theta) : -1.0f;

    if(radius == 0.0f) return 0.0f;
    return 1.0f / (2.0f * M_PI * (1.0f - cos_theta));
}

float sample_directional_light_pdf(directional_light l)
{
    return l.solid_angle == 0.0f ? 0.0f : 1.0f / (2.0f * M_PI * l.solid_angle);
}

void sample_directional_light(
    directional_light l,
    vec2 u,
    out vec3 dir,
    out vec3 color,
    out float pdf
){
    color = rgbe_to_rgb(l.color);
    dir = sample_cone(
        -octahedral_decode(unpackSnorm2x16(l.direction)),
        1.0f - l.solid_angle, u
    );
    pdf = sample_directional_light_pdf(l);
    if(pdf != 0.0f) color *= pdf;
}

float sample_tri_light_pdf(tri_light l, vec3 pos)
{
    return 1.0f / triangle_solid_angle(
        pos,
        l.corners[0].xyz,
        l.corners[1].xyz,
        l.corners[2].xyz
    );
}

#ifdef RAYBASE_SCENE_SET
vec3 sample_tri_light(
    tri_light l,
    vec2 u,
    vec3 pos,
    out vec3 dir,
    out float dist,
    out vec3 color,
    out float pdf
){
    sample_triangle_solid_angle(
        u, pos,
        l.corners[0].xyz,
        l.corners[1].xyz,
        l.corners[2].xyz,
        dir,
        pdf
    );

    dist = ray_plane_intersection(
        l.corners[0].xyz,
        l.corners[1].xyz,
        l.corners[2].xyz,
        pos, dir
    );

    vec3 bary = triangle_barycentric_coords(
        pos + dir * dist,
        l.corners[0].xyz,
        l.corners[1].xyz,
        l.corners[2].xyz
    );

    color = rgbe_to_rgb(l.emission_factor);
    if(l.emission_tex != UNUSED_TEXTURE)
    {
        vec2 uv0 = unpackHalf2x16(floatBitsToUint(l.corners[0].w));
        vec2 uv1 = unpackHalf2x16(floatBitsToUint(l.corners[1].w));
        vec2 uv2 = unpackHalf2x16(floatBitsToUint(l.corners[2].w));

        vec2 uv = uv0 * bary.x + uv1 * bary.y + uv2 * bary.z;
        color *= inverse_srgb_correction(textureLod(textures[nonuniformEXT(l.emission_tex&0xFFFF)], uv, 0.0f).rgb);
    }
    return bary;
}
#endif

#endif
