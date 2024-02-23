#include "ray_tracing_pipeline.hh"
#include "vulkan_helpers.hh"

namespace rb::gfx
{

ray_tracing_pipeline::ray_tracing_pipeline(device& dev)
: gpu_pipeline(dev, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR)
{
}

void ray_tracing_pipeline::init(
    const ray_tracing_shader_data& program,
    size_t push_constant_size,
    argvec<VkDescriptorSetLayout> sets
){
    std::vector<vkres<VkShaderModule>> shader_modules;
    std::vector<VkPipelineShaderStageCreateInfo> shader_infos;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;

    specialization_info_wrapper generation_info(program.generation.specialization);
    shader_modules.push_back(load_shader(program.generation));
    shader_infos.push_back({
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr, {}, VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        shader_modules.back(), "main", &generation_info.info
    });
    shader_groups.push_back({
        VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        nullptr,
        VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        (uint32_t)(shader_modules.size()-1),
        VK_SHADER_UNUSED_KHR,
        VK_SHADER_UNUSED_KHR,
        VK_SHADER_UNUSED_KHR,
        nullptr
    });

    specialization_info_wrapper miss_info(program.miss.specialization);
    if(program.miss.data)
    {
        shader_modules.push_back(load_shader(program.miss));
        shader_infos.push_back({
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr, {}, VK_SHADER_STAGE_MISS_BIT_KHR,
            shader_modules.back(), "main", &miss_info.info
        });
        shader_groups.push_back({
            VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            nullptr,
            VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            (uint32_t)(shader_modules.size()-1),
            VK_SHADER_UNUSED_KHR,
            VK_SHADER_UNUSED_KHR,
            VK_SHADER_UNUSED_KHR,
            nullptr
        });
    }

    std::vector<specialization_info_wrapper> hg_infos;
    hg_infos.reserve(program.hit_groups.size() * 3);
    for(const hit_group& hg: program.hit_groups)
    {
        int closest_hit = -1;
        if(hg.closest_hit.data)
        {
            shader_modules.push_back(load_shader(hg.closest_hit));
            shader_infos.push_back({
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                nullptr, {}, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                shader_modules.back(), "main", &hg_infos.emplace_back(hg.closest_hit.specialization).info
            });
            closest_hit = shader_modules.size()-1;
        }

        int any_hit = -1;
        if(hg.any_hit.data)
        {
            shader_modules.push_back(load_shader(hg.any_hit));
            shader_infos.push_back({
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                nullptr, {}, VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                shader_modules.back(), "main",
                &hg_infos.emplace_back(hg.any_hit.specialization).info
            });
            any_hit = shader_modules.size()-1;
        }

        int intersection = -1;
        if(hg.intersection.data)
        {
            shader_modules.push_back(load_shader(hg.intersection));
            shader_infos.push_back({
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                nullptr, {}, VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
                shader_modules.back(), "main",
                &hg_infos.emplace_back(hg.intersection.specialization).info
            });
            intersection = shader_modules.size()-1;
        }

        shader_groups.push_back({
            VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            nullptr,
            hg.type,
            VK_SHADER_UNUSED_KHR,
            closest_hit >= 0 ? closest_hit : VK_SHADER_UNUSED_KHR,
            any_hit >= 0 ? any_hit : VK_SHADER_UNUSED_KHR,
            intersection >= 0 ? intersection : VK_SHADER_UNUSED_KHR,
            nullptr
        });
    }

    init_bindings(push_constant_size, sets);

    VkPipeline pipeline;
    VkRayTracingPipelineCreateInfoKHR pipeline_info = {
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        nullptr,
        {},
        (uint32_t)shader_infos.size(),
        shader_infos.data(),
        (uint32_t)shader_groups.size(),
        shader_groups.data(),
        program.max_recursion,
        nullptr,
        nullptr,
        nullptr,
        *pipeline_layout,
        VK_NULL_HANDLE,
        -1
    };
    vkCreateRayTracingPipelinesKHR(
        dev->logical_device,
        VK_NULL_HANDLE,
        dev->pp_cache,
        1,
        &pipeline_info,
        nullptr,
        &pipeline
    );
    this->pipeline = vkres(*dev, pipeline);
    dev->gc.depend(pipeline_layout, pipeline);

    uint32_t aligned_handle_size = align_up_to(
        dev->rp_properties.shaderGroupHandleSize,
        dev->rp_properties.shaderGroupHandleAlignment
    );
    gen_region.stride = align_up_to(aligned_handle_size, dev->rp_properties.shaderGroupBaseAlignment);
    gen_region.size = gen_region.stride;
    miss_region.stride = aligned_handle_size;
    miss_region.size = program.miss.data ? align_up_to(aligned_handle_size, dev->rp_properties.shaderGroupBaseAlignment) : 0;
    hit_region.stride = aligned_handle_size;
    hit_region.size = align_up_to(
        program.hit_groups.size() * aligned_handle_size,
        dev->rp_properties.shaderGroupBaseAlignment
    );
    call_region.stride = aligned_handle_size;
    call_region.size = 0;

    std::vector<uint8_t> handles(
        dev->rp_properties.shaderGroupHandleSize * shader_groups.size()
    );
    vkGetRayTracingShaderGroupHandlesKHR(
        dev->logical_device,
        pipeline,
        0,
        shader_groups.size(),
        handles.size(),
        handles.data()
    );

    size_t sbt_size = gen_region.size + miss_region.size + hit_region.size + call_region.size;
    std::vector<uint8_t> sbt_data(sbt_size);
    uint32_t handle_index = 0;
    // Generation
    uint8_t* region_data = sbt_data.data();
    memcpy(
        region_data,
        handles.data() + handle_index * dev->rp_properties.shaderGroupHandleSize,
        dev->rp_properties.shaderGroupHandleSize
    );
    handle_index++;
    region_data += gen_region.size;

    // Miss
    if(program.miss.data)
    {
        memcpy(
            region_data,
            handles.data() + handle_index * dev->rp_properties.shaderGroupHandleSize,
            dev->rp_properties.shaderGroupHandleSize
        );
        handle_index++;
        region_data += miss_region.size;
    }

    // Hit groups
    for(size_t i = 0; i < program.hit_groups.size(); ++i)
    {
        memcpy(
            region_data,
            handles.data() + handle_index * dev->rp_properties.shaderGroupHandleSize,
            dev->rp_properties.shaderGroupHandleSize
        );
        handle_index++;
        region_data += hit_region.stride;
    }

    event upload_event;
    sbt_buffer = upload_buffer(
        *dev, upload_event,
        sbt_data.size(),
        sbt_data.data(),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
    );
    upload_event.wait(*dev);
    VkBufferDeviceAddressInfo address_info = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        nullptr,
        *sbt_buffer
    };
    VkDeviceAddress sbt_address = vkGetBufferDeviceAddress(dev->logical_device, &address_info);

    gen_region.deviceAddress = sbt_address;
    miss_region.deviceAddress = sbt_address + gen_region.size;
    hit_region.deviceAddress = sbt_address + gen_region.size + miss_region.size;
    call_region.deviceAddress = 0;

    dev->gc.depend(*sbt_buffer, pipeline);
}

void ray_tracing_pipeline::trace_rays(VkCommandBuffer buf, uvec3 work_size)
{
    vkCmdTraceRaysKHR(
        buf, &gen_region, &miss_region, &hit_region, &call_region,
        work_size.x, work_size.y, work_size.z
    );
}

}
