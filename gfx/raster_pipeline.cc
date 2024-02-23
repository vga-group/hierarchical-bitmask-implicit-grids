#include "raster_pipeline.hh"
#include "render_pass.hh"
#include "vulkan_helpers.hh"

namespace rb::gfx
{

raster_pipeline::raster_pipeline(device& dev)
: gpu_pipeline(dev, VK_PIPELINE_BIND_POINT_GRAPHICS)
{
}

raster_pipeline::params::params(
    const render_pass& rp,
    uint32_t subpass_index,
    ivec2 size,
    size_t binding_count,
    const VkVertexInputBindingDescription* bindings,
    size_t attribute_count,
    const VkVertexInputAttributeDescription* attributes
): rp(&rp), subpass_index(subpass_index)
{
    vertex_input_info = VkPipelineVertexInputStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        0,
        (uint32_t)binding_count,
        bindings,
        (uint32_t)attribute_count,
        attributes
    };

    input_assembly_info = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        nullptr,
        0,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_FALSE
    };

    uvec2 usize = abs(size);
    viewport = {0.f, float(usize.y), float(usize.x), -float(usize.y), 0.f, 1.f};
    scissor = {{0, 0}, {usize.x, usize.y}};

    rasterization_info = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        nullptr,
        0,
        VK_FALSE,
        VK_FALSE,
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_BACK_BIT,
        VK_FRONT_FACE_COUNTER_CLOCKWISE,
        VK_FALSE, 0.0f, 0.0f, 0.0f,
        0.0f
    };

    multisample_info = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0,
        rp.get_samples(),
        VK_FALSE,
        1.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE
    };

    static constexpr VkDynamicState unsized_dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    dynamic_info = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0,
        uint32_t(any(lessThan(size, ivec2(0))) ? std::size(unsized_dynamic_states) : 0),
        unsized_dynamic_states
    };

    bool has_depth = rp.subpass_has_depth(subpass_index);

    depth_stencil_info = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        0,
        has_depth ? VK_TRUE : VK_FALSE,
        has_depth ? VK_TRUE : VK_FALSE,
        VK_COMPARE_OP_GREATER_OR_EQUAL,
        VK_FALSE,
        VK_FALSE,
        {},
        {},
        0.0f, 1.0f
    };

    blend_states = std::vector(
        rp.subpass_target_count(subpass_index) - (has_depth ? 1:0),
        VkPipelineColorBlendAttachmentState{
            VK_FALSE,
            VK_BLEND_FACTOR_SRC_ALPHA,
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|
            VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
        }
    );

    conservative_rasterization_info = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,
        nullptr,
        0,
        VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT,
        0.0f
    };
}

void raster_pipeline::init(
    const params& p,
    const raster_shader_data& sd,
    size_t push_constant_size,
    argvec<VkDescriptorSetLayout> sets
){
    params create_params = p;

    // AMD Fix: MSAA is broken by default, so force sample shading with the matching minSampleShading
    if (dev->physical_device_props.properties.vendorID == 4098)
    {
        create_params.multisample_info.sampleShadingEnable = VK_TRUE;
        create_params.multisample_info.minSampleShading = clamp(1.0f / create_params.multisample_info.rasterizationSamples + 0.01f, 0.0f, 1.0f);
    }

    if(dev->supports_conservative_rasterization)
        create_params.rasterization_info.pNext = &create_params.conservative_rasterization_info;

    // Load shaders
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    vkres<VkShaderModule> vertex_shader = load_shader(sd.vertex);
    specialization_info_wrapper vertex_spec(sd.vertex.specialization);

    if(vertex_shader != VK_NULL_HANDLE)
    {
        stages.push_back({
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr, {}, VK_SHADER_STAGE_VERTEX_BIT, vertex_shader,
            "main", &vertex_spec.info
        });
    }

    vkres<VkShaderModule> fragment_shader = load_shader(sd.fragment);
    specialization_info_wrapper fragment_spec(sd.fragment.specialization);

    if(fragment_shader != VK_NULL_HANDLE)
    {
        stages.push_back({
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr, {}, VK_SHADER_STAGE_FRAGMENT_BIT,
            fragment_shader, "main", &fragment_spec.info
        });
    }

    // Init bindings now that we have info from the shaders.
    init_bindings(push_constant_size, sets);

    // Setup fixed function structs
    VkPipelineViewportStateCreateInfo viewport_info = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        nullptr, 0, 1, &create_params.viewport, 1, &create_params.scissor
    };

    VkPipelineColorBlendStateCreateInfo blend_info = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        nullptr,
        0,
        VK_FALSE,
        VK_LOGIC_OP_COPY,
        uint32_t(create_params.blend_states.size()),
        create_params.blend_states.data(),
        {0.0f, 0.0f, 0.0f, 0.0f}
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        nullptr,
        0,
        (uint32_t)stages.size(),
        stages.data(),
        &create_params.vertex_input_info,
        &create_params.input_assembly_info,
        nullptr,
        &viewport_info,
        &create_params.rasterization_info,
        &create_params.multisample_info,
        create_params.depth_stencil_info.depthTestEnable == false && create_params.depth_stencil_info.depthWriteEnable == false && create_params.depth_stencil_info.stencilTestEnable == false ?
            nullptr : &create_params.depth_stencil_info,
        &blend_info,
        &create_params.dynamic_info,
        pipeline_layout,
        *create_params.rp,
        create_params.subpass_index,
        VK_NULL_HANDLE,
        -1
    };

    VkPipeline pipeline;
    vkCreateGraphicsPipelines(
        dev->logical_device,
        dev->pp_cache,
        1,
        &pipeline_info,
        nullptr,
        &pipeline
    );
    this->pipeline = vkres(*dev, pipeline);
    dev->gc.depend(pipeline_layout, pipeline);
    dev->gc.depend((VkRenderPass)*create_params.rp, pipeline);
}

}
