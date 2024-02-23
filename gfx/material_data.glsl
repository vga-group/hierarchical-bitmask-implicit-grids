#ifndef RAYBASE_GFX_MATERIAL_DATA_GLSL
#define RAYBASE_GFX_MATERIAL_DATA_GLSL

#define UNUSED_TEXTURE 0xFFFF

struct material_spec
{
    // FACTORS
    // xyz = color, w = alpha (8-bit unorm)
    uint color;
    // x = metallic, y = roughness, z = transmission, w = translucency (8-bit unorm)
    uint metallic_roughness_transmission_translucency;
    // xyzw = emission (RGBE, 8-bit unorm)
    uint emission;
    // xyz = transmission attenuation, w = (ior-1)/2 (8-bit unorm)
    uint attenuation_ior;
    // x = 1 if double sided, y = alpha cutoff, z = unused, w = unused (8-bit unorm)
    uint double_sided_cutoff;
    // x = clearcoat factor, y = clearcoat roughness, z = anisotropy, w = anisotropy rotation (8-bit unorm)
    uint clearcoat_factor_roughness_anisotropy;
    // xyz = sheen color, w = sheen roughness (8-bit unorm)
    uint sheen_color_roughness;
    // x, y, z = subsurface radius per color, w = subsurface strength
    uint subsurface;

    // TEXTURES (0xFFFF means unset!)
    // x = color + alpha, y = metallic+roughness (uint16_t)
    uint color_metallic_roughness_textures;
    // x = normal, y = emission (uint16_t)
    uint normal_emission_textures;
    // x = clearcoat factor + clearcoat roughness, y = clearcoat normal (uint16_t)
    uint clearcoat_textures;
    // x = transmission + translucency, y = sheen (uint16_t)
    uint transmission_sheen_textures;
};

struct material
{
    vec4 albedo;
    float metallic;
    float roughness;
    float transmission;
    float translucency;
    vec3 emission;
    vec3 attenuation;
    float eta;
    float f0;
    float clearcoat;
    float clearcoat_roughness;
    vec3 sheen_color;
    float sheen_roughness;
    float anisotropy;
    float anisotropy_rotation;
    vec3 normal;
    vec3 clearcoat_normal;
};

void init_black_material(out material mat)
{
    mat.albedo = vec4(0,0,0,1);
    mat.metallic = 1.0f;
    mat.roughness = 1.0f;
    mat.transmission = 0.0f;
    mat.translucency = 0.0f;
    mat.emission = vec3(0);
    mat.attenuation = vec3(1.0f);
    mat.eta = 1.0f;
    mat.f0 = 0;
    mat.clearcoat = 0.0f;
    mat.clearcoat_roughness = 0.0f;
    mat.sheen_color = vec3(0.0f);
    mat.sheen_roughness = 0.0f;
    mat.anisotropy = 0;
    mat.anisotropy_rotation = 0;
    mat.normal = vec3(0,0,1);
    mat.clearcoat_normal = vec3(0,0,1);
}

#endif
