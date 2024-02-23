#include "material.hh"
#include "texture.hh"

namespace rb::gfx
{

bool material::potentially_transparent() const
{
    return color.a < 1.0f ||
        (color_texture.second && color_texture.second->potentially_transparent());
}

bool material::potentially_emissive() const
{
    return emission.r != 0.0f || emission.g != 0.0f || emission.b != 0.0f;
}

material::material_id material::get_material_id() const
{
    size_t seed = 0;
    hash_combine(seed, color);
    hash_combine(seed, color_texture);
    hash_combine(seed, metallic);
    hash_combine(seed, roughness);
    hash_combine(seed, metallic_roughness_texture);
    hash_combine(seed, normal_texture);
    hash_combine(seed, ior);
    hash_combine(seed, emission);
    hash_combine(seed, emission_texture);
    hash_combine(seed, transmission);
    hash_combine(seed, translucency);
    hash_combine(seed, transmission_translucency_texture);
    hash_combine(seed, volume_attenuation);
    hash_combine(seed, clearcoat);
    hash_combine(seed, clearcoat_roughness);
    hash_combine(seed, clearcoat_texture);
    hash_combine(seed, clearcoat_normal_texture);
    hash_combine(seed, anisotropy);
    hash_combine(seed, anisotropy_rotation);
    hash_combine(seed, sheen_color);
    hash_combine(seed, sheen_roughness);
    hash_combine(seed, sheen_texture);
    hash_combine(seed, double_sided);
    hash_combine(seed, alpha_cutoff);
    hash_combine(seed, stencil_reference);
    hash_combine(seed, casts_shadows);
    hash_combine(seed, force_envmap);
    return seed;
}

bool material::operator==(const material& other) const
{
    return
        color == other.color &&
        color_texture == other.color_texture &&
        metallic == other.metallic &&
        roughness == other.roughness &&
        metallic_roughness_texture == other.metallic_roughness_texture &&
        normal_texture == other.normal_texture &&
        ior == other.ior &&
        emission == other.emission &&
        emission_texture == other.emission_texture &&
        transmission == other.transmission &&
        translucency == other.translucency &&
        transmission_translucency_texture == other.transmission_translucency_texture &&
        volume_attenuation == other.volume_attenuation &&
        clearcoat == other.clearcoat &&
        clearcoat_roughness == other.clearcoat_roughness &&
        clearcoat_texture == other.clearcoat_texture &&
        clearcoat_normal_texture == other.clearcoat_normal_texture &&
        anisotropy == other.anisotropy &&
        anisotropy_rotation == other.anisotropy_rotation &&
        sheen_color == other.sheen_color &&
        sheen_roughness == other.sheen_roughness &&
        sheen_texture == other.sheen_texture &&
        double_sided == other.double_sided &&
        alpha_cutoff == other.alpha_cutoff &&
        stencil_reference == other.stencil_reference &&
        casts_shadows == other.casts_shadows &&
        force_envmap == other.force_envmap;
}

}

size_t std::hash<rb::gfx::material::sampler_tex>::operator()(const rb::gfx::material::sampler_tex& v) const
{
    size_t a = std::hash<const rb::gfx::sampler*>()(v.first);
    size_t b = std::hash<const rb::gfx::texture*>()(v.second);
    return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
}
