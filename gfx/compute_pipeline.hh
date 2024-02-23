#ifndef RAYBASE_GFX_COMPUTE_PIPELINE_HH
#define RAYBASE_GFX_COMPUTE_PIPELINE_HH

#include "gpu_pipeline.hh"
#include "core/math.hh"

namespace rb::gfx
{

class compute_pipeline: public gpu_pipeline
{
public:
    compute_pipeline(device& dev);

    void init(
        shader_data compute,
        size_t push_constant_size = 0,
        argvec<VkDescriptorSetLayout> sets = {}
    );

    void dispatch(VkCommandBuffer buf, uvec3 work_size);
};

}

#endif
