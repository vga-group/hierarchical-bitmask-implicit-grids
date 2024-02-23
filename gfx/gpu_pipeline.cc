#include "gpu_pipeline.hh"
#include "vulkan_helpers.hh"
#include "core/error.hh"
#include "spirv_reflect.h"
#include "texture.hh"
#include "sampler.hh"
#include "core/stack_allocator.hh"

namespace rb::gfx
{

specialization_info_wrapper::specialization_info_wrapper(const specialization_info& info)
{
    entries.reserve(info.size());
    data.reserve(info.size());
    uint32_t offset = 0;
    for(const auto& pair: info)
    {
        entries.push_back(VkSpecializationMapEntry{pair.first, offset, sizeof(uint32_t)});
        data.push_back(pair.second);
        offset += sizeof(uint32_t);
    }
    this->info.mapEntryCount = entries.size();
    this->info.pMapEntries = entries.data();
    this->info.dataSize = data.size() * sizeof(uint32_t);
    this->info.pData = data.data();
}

std::vector<shader_registry_entry>& get_shader_registry()
{
    static std::vector<shader_registry_entry> registry;
    return registry;
}

registry_helper::registry_helper(const uint32_t* data, const char* binary_path, const char* source_path)
{
    get_shader_registry().push_back({binary_path, source_path, data});
}

gpu_pipeline::gpu_pipeline(device& dev, VkPipelineBindPoint bind_point)
: dev(&dev), bind_point(bind_point)
{
}

gpu_pipeline::~gpu_pipeline()
{
}

void gpu_pipeline::init_bindings(
    size_t push_constant_size,
    argvec<VkDescriptorSetLayout> sets
){
    this->push_constant_size = push_constant_size;

    VkDevice logical_device = dev->logical_device;

    VkPushConstantRange range = {
        VK_SHADER_STAGE_ALL, 0, (uint32_t)push_constant_size
    };

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        nullptr, {},
        (uint32_t)sets.size(), sets.begin(),
        push_constant_size ? 1u : 0u,
        &range
    };
    VkPipelineLayout tmp_layout;
    vkCreatePipelineLayout(
        logical_device, &pipeline_layout_info, nullptr, &tmp_layout
    );
    pipeline_layout = vkres(*dev, tmp_layout);
    dev->gc.depend_many(
        argvec(sets).make_void_ptr(),
        *pipeline_layout
    );
}

void gpu_pipeline::push_constants(VkCommandBuffer buf, const void* data)
{
    vkCmdPushConstants(
        buf, pipeline_layout, VK_SHADER_STAGE_ALL, 0,
        (uint32_t)push_constant_size, data
    );
}

void gpu_pipeline::bind(VkCommandBuffer buf)
{
    vkCmdBindPipeline(buf, bind_point, pipeline);
    dev->gc.depend(*pipeline, buf);
}

void gpu_pipeline::set_descriptors(
    VkCommandBuffer buf,
    const descriptor_set& set,
    uint32_t index,
    uint32_t set_index
){
    set.bind(buf, *pipeline_layout, bind_point, index, set_index);
}

void gpu_pipeline::push_descriptors(
    VkCommandBuffer buf,
    push_descriptor_set& set,
    uint32_t set_index
){
    set.push(buf, *pipeline_layout, bind_point, set_index);
}

vkres<VkShaderModule> gpu_pipeline::load_shader(shader_data data)
{
    if(data.bytes == 0) return {};

    get_hooked_shader(data);

    VkShaderModuleCreateInfo create_info{
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        nullptr,
        0,
        data.bytes,
        data.data
    };
    VkShaderModule mod;
    vkCreateShaderModule(
        dev->logical_device,
        &create_info,
        nullptr,
        &mod
    );

    return {*dev, mod};
}

void gpu_pipeline::add_shader_load_hook(
    std::function<void(shader_data&)>&& hook
){
    shader_load_callback = [
        old_callback = std::move(shader_load_callback),
        new_callback = std::move(hook)
    ](shader_data& data){
        if(old_callback) old_callback(data);
        new_callback(data);
    };
}

void gpu_pipeline::get_hooked_shader(shader_data& data)
{
    if(shader_load_callback)
        shader_load_callback(data);
}

std::function<void(shader_data&)> gpu_pipeline::shader_load_callback;

}
