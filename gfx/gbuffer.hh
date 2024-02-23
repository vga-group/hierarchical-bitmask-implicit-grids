#ifndef RAYBASE_GFX_GBUFFER_HH
#define RAYBASE_GFX_GBUFFER_HH

#include "render_target.hh"
#include "texture.hh"
#include <optional>

namespace rb::gfx
{

class render_pass;

template<typename T>
struct gbuffer_entries
{
    // Expected format: RGBA16F or RGBA32F
    // RGB = Color in linear color space
    // A = Alpha channel
    T color;

    // Expected format: any depth format
    // A depth buffer with the reverse Z mapping:
    // https://developer.nvidia.com/content/depth-precision-visualized
    T depth;

    // Expected format: RG16_SNORM or RGBA16_SNORM
    // RG = Normal in octahedral basis
    // BA (if present) = Tangent in octahedral basis
    // Bitangent is not provided as this buffer already has normal mapping
    // applied; it no longer matters. The tangent is only useful for
    // anisotropy.
    T normal;

    // Expected format: RG16_SNORM
    // RG = Flat normal in octahedral basis
    T flat_normal;

    // Expected format: RGBA32_SFLOAT
    // RGB = World-position OR view direction
    // A = 0.0f if direction, 1.0f if world-position
    // Gives a view direction whenever a ray has missed the scene.
    T position;

    // Expected format: RGBA8_UNORM
    // Albedo in RGB channels, alpha in A.
    T albedo;

    // Expected format: R32_UINT
    // Emission in RGBE encoding.
    T emission;

    // Expected format: RGBA8_UNORM
    // R = Metallic
    // G = Roughness
    // B = ETA*0.5f
    // A = Translucency
    T material;

    // Expected format: RGBA8_UNORM
    // R,G,B = Attenuation
    // A = Transmission
    T transmission;

    // Expected format: RGBA8_UNORM
    // R,G,B = Sheen color
    // A = Sheen roughness
    T sheen;

    // Expected format: RGBA8_UNORM
    // R = Clearcoat
    // G = Clearcoat roughness
    // B = Anisotropy
    // A = Anisotropy rotation
    T material_extra;

    // Expected format: R32_UINT or RG32_UINT
    // R = Previous screen UV coordinate packUnorm2x16()
    // G = Previous depth buffer value (floatBitsToUint())
    // Only exact when stochastic anti-aliasing is not present in the used
    // rendering algorithms.
    T motion;
};

struct gbuffer_target: gbuffer_entries<std::optional<render_target>> {};

struct gbuffer
{
    static constexpr uint32_t COLOR = (1<<0);
    static constexpr uint32_t DEPTH = (1<<1);
    static constexpr uint32_t NORMAL = (1<<2);
    static constexpr uint32_t TANGENT = (1<<3);
    static constexpr uint32_t FLAT_NORMAL = (1<<4);
    static constexpr uint32_t POSITION = (1<<5);
    static constexpr uint32_t ALBEDO = (1<<6);
    static constexpr uint32_t EMISSION = (1<<7);
    static constexpr uint32_t MATERIAL = (1<<8);
    static constexpr uint32_t TRANSMISSION = (1<<9);
    static constexpr uint32_t SHEEN = (1<<10);
    static constexpr uint32_t MATERIAL_EXTRA = (1<<11);
    static constexpr uint32_t MOTION = (1<<12);

    gbuffer() = default;
    gbuffer(device& dev, uvec2 size, uint32_t buffer_mask, bool temporal = false);
    gbuffer(device& dev, uvec2 size, uint32_t buffer_mask, uint32_t temporal_mask);

    void init(device& dev, uvec2 size, uint32_t buffer_mask, bool temporal = false);
    void init(device& dev, uvec2 size, uint32_t buffer_mask, uint32_t temporal_mask);

    gbuffer_target get_target();
    gbuffer_target get_temporal_target();

    gbuffer_entries<std::optional<texture>> textures;
};

}

#endif
