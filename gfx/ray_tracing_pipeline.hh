#ifndef RAYBASE_GFX_RAY_TRACING_PIPELINE_HH
#define RAYBASE_GFX_RAY_TRACING_PIPELINE_HH

#include "gpu_pipeline.hh"
#include "render_target.hh"

namespace rb::gfx
{

struct hit_group
{
    VkRayTracingShaderGroupTypeKHR type =
        VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    shader_data closest_hit;
    shader_data any_hit;
    shader_data intersection;
};

struct ray_tracing_shader_data
{
    shader_data generation;
    std::vector<hit_group> hit_groups;
    shader_data miss;
    unsigned max_recursion = 1;
};

class ray_tracing_pipeline: public gpu_pipeline
{
public:
    ray_tracing_pipeline(device& dev);

    void init(
        const ray_tracing_shader_data& program,
        size_t push_constant_size = 0,
        argvec<VkDescriptorSetLayout> sets = {}
    );

    void trace_rays(VkCommandBuffer buf, uvec3 work_size);

private:
    vkres<VkBuffer> sbt_buffer;
    VkStridedDeviceAddressRegionKHR gen_region;
    VkStridedDeviceAddressRegionKHR miss_region;
    VkStridedDeviceAddressRegionKHR hit_region;
    VkStridedDeviceAddressRegionKHR call_region;
};

}

#endif
