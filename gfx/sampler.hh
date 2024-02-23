#ifndef RAYBASE_GFX_SAMPLER_HH
#define RAYBASE_GFX_SAMPLER_HH
#include "device.hh"
#include "vkres.hh"

namespace rb::gfx
{

class sampler
{
public:
    sampler(
        device& dev,
        VkFilter min = VK_FILTER_LINEAR,
        VkFilter mag = VK_FILTER_LINEAR,
        VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VkSamplerAddressMode extension = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        float anisotropy = 16,
        float max_mipmap = 100.0f,
        float mipmap_bias = 0.0f,
        bool shadow = false
    );
    sampler(const sampler& other) = delete;
    sampler(sampler&& other) noexcept = default;

    VkSampler get() const;

private:
    vkres<VkSampler> sampler_object;
};

}

#endif
