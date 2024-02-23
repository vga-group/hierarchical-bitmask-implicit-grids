#ifndef RAYBASE_GFX_RASTER_PIPELINE_HH
#define RAYBASE_GFX_RASTER_PIPELINE_HH

#include "gpu_pipeline.hh"
#include "render_target.hh"

namespace rb::gfx
{

struct raster_shader_data
{
    shader_data vertex;
    shader_data fragment;
};

class render_pass;

class raster_pipeline: public gpu_pipeline
{
public:
    raster_pipeline(device& dev);

    struct params
    {
        params() = default;
        params(
            const render_pass& rp,
            uint32_t subpass_index,
            ivec2 size = ivec2(-1, -1), // Negative size causes dynamic state
            size_t binding_count = 0,
            const VkVertexInputBindingDescription* bindings = nullptr,
            size_t attribute_count = 0,
            const VkVertexInputAttributeDescription* attributes = nullptr
        );

        const render_pass* rp;
        uint32_t subpass_index;

        // These are all auto-filled to reasonable defaults; you can modify
        // them afterwards to suit your specific needs.
        VkPipelineVertexInputStateCreateInfo vertex_input_info;
        VkPipelineInputAssemblyStateCreateInfo input_assembly_info;
        VkViewport viewport;
        VkRect2D scissor;
        VkPipelineRasterizationStateCreateInfo rasterization_info;
        VkPipelineMultisampleStateCreateInfo multisample_info;
        VkPipelineDepthStencilStateCreateInfo depth_stencil_info;
        VkPipelineDynamicStateCreateInfo dynamic_info;
        VkPipelineRasterizationConservativeStateCreateInfoEXT conservative_rasterization_info;

        std::vector<VkPipelineColorBlendAttachmentState> blend_states;
    };

    void init(
        const params& p,
        const raster_shader_data& program,
        size_t push_constant_size = 0,
        argvec<VkDescriptorSetLayout> sets = {}
    );
};

}

#endif
