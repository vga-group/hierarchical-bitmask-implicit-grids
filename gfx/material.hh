#ifndef RAYBASE_GFX_MATERIAL_HH
#define RAYBASE_GFX_MATERIAL_HH
#include "core/types.hh"
#include "core/math.hh"
#include "core/ecs.hh"

namespace rb::gfx
{

class texture;
class sampler;
class environment_map;

// You can enable/disable material features when rendering in order to improve
// performance.
constexpr uint32_t MATERIAL_FEATURE_ALBEDO = 1<<0;
constexpr uint32_t MATERIAL_FEATURE_METALLIC_ROUGHNESS = 1<<1;
constexpr uint32_t MATERIAL_FEATURE_EMISSION = 1<<2;
constexpr uint32_t MATERIAL_FEATURE_NORMAL_MAP = 1<<3;
constexpr uint32_t MATERIAL_FEATURE_TRANSMISSION = 1<<4;
constexpr uint32_t MATERIAL_FEATURE_ATTENUATION = 1<<5;
constexpr uint32_t MATERIAL_FEATURE_CLEARCOAT = 1<<6;
constexpr uint32_t MATERIAL_FEATURE_SHEEN = 1<<7;
constexpr uint32_t MATERIAL_FEATURE_SUBSURFACE = 1<<8;
constexpr uint32_t MATERIAL_FEATURE_ANISOTROPY = 1<<9;
constexpr uint32_t MATERIAL_FEATURE_MULTISCATTER = 1<<10;
constexpr uint32_t MATERIAL_FEATURE_ENVMAP_SPECULAR = 1<<11;
constexpr uint32_t MATERIAL_FEATURE_ENVMAP_PARALLAX = 1<<12;
constexpr uint32_t MATERIAL_FEATURE_LIGHTMAP = 1<<13;
constexpr uint32_t MATERIAL_FEATURE_SH_PROBES = 1<<14;

struct material
{
    using sampler_tex = std::pair<const sampler*, const texture*>;

    vec4 color = vec4(1);
    sampler_tex color_texture = {nullptr, nullptr};

    float metallic = 0.0f;
    float roughness = 1.0f;

    // metalness on B-channel, roughness on G-channel.
    sampler_tex metallic_roughness_texture = {nullptr, nullptr};
    sampler_tex normal_texture = {nullptr, nullptr};

    float ior = 1.43f;

    vec3 emission = vec3(0);
    sampler_tex emission_texture = {nullptr, nullptr};

    float transmission = 0.0f;
    float translucency = 0.0f; // Ratio of diffuse to specular transmission
    // transmission on R-channel, translucency on G-channel.
    sampler_tex transmission_translucency_texture = {nullptr, nullptr};

    vec3 volume_attenuation = vec3(1.0f);

    float clearcoat = 0.0f;
    float clearcoat_roughness = 0.0f;
    sampler_tex clearcoat_texture = {nullptr, nullptr};
    sampler_tex clearcoat_normal_texture = {nullptr, nullptr};

    float anisotropy = 0.0f;
    float anisotropy_rotation = 0.0f;

    vec3 sheen_color = vec3(0.0f);
    float sheen_roughness = 1.0f;
    sampler_tex sheen_texture = {nullptr, nullptr};

    // true if back faces should be rendered.
    bool double_sided = false;
    // When applicable, value for clipping alpha.
    float alpha_cutoff = -1.0f;
    // When applicable, value for the stencil buffer write or compare ops.
    // Exact behaviour depends on renderer settings.
    uint32_t stencil_reference = 0xFFFFFFFF;

    // false if should be skipped in shadow maps.
    bool casts_shadows = true;

    // You can force a specific envmap with this, otherwise you get an
    // auto-assigned one.
    entity force_envmap = INVALID_ENTITY;

    bool potentially_transparent() const;
    bool potentially_emissive() const;

    // Material ID is just a hash, we just hope there are no collisions ;)
    // (they are extremely unlikely)
    using material_id = size_t;
    material_id get_material_id() const;

    bool operator==(const material& other) const;
};

}

namespace std
{
    template<> struct hash<rb::gfx::material::sampler_tex>
    {
        size_t operator()(const rb::gfx::material::sampler_tex& v) const;
    };
}

#endif
