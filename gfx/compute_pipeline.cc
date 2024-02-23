#include "compute_pipeline.hh"
#include "vulkan_helpers.hh"

namespace rb::gfx
{

compute_pipeline::compute_pipeline(device& dev)
: gpu_pipeline(dev, VK_PIPELINE_BIND_POINT_COMPUTE)
{
}

void compute_pipeline::init(
    shader_data compute,
    size_t push_constant_size,
    argvec<VkDescriptorSetLayout> sets
){
    vkres<VkShaderModule> shader = load_shader(compute);
    specialization_info_wrapper compute_spec(compute.specialization);

    VkPipelineShaderStageCreateInfo shader_info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        {},
        VK_SHADER_STAGE_COMPUTE_BIT,
        shader,
        "main",
        &compute_spec.info
    };

    init_bindings(push_constant_size, sets);
    VkComputePipelineCreateInfo pipeline_info = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr,
        {},
        shader_info,
        pipeline_layout,
        VK_NULL_HANDLE,
        0
    };
    VkPipeline pipeline;
    vkCreateComputePipelines(
        dev->logical_device,
        dev->pp_cache,
        1,
        &pipeline_info,
        nullptr,
        &pipeline
    );
    this->pipeline = vkres(*dev, pipeline);
    dev->gc.depend(pipeline_layout, pipeline);
}

void compute_pipeline::dispatch(VkCommandBuffer buf, uvec3 work_size)
{
    vkCmdDispatch(buf, work_size.x, work_size.y, work_size.z);
}

}
