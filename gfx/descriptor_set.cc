#include "descriptor_set.hh"
#include "gpu_pipeline.hh"
#include "raster_pipeline.hh"
#include "ray_tracing_pipeline.hh"
#include "vulkan_helpers.hh"
#include "core/error.hh"
#include "spirv_reflect.h"
#include "texture.hh"
#include "sampler.hh"
#include "core/stack_allocator.hh"

namespace rb::gfx
{

descriptor_set_layout::descriptor_set_layout(device& dev, bool push_descriptor_set)
: dev(&dev), push_descriptor_set(push_descriptor_set)
{
}

descriptor_set_layout::~descriptor_set_layout() {}

void descriptor_set_layout::add(
    std::string_view name,
    const VkDescriptorSetLayoutBinding& binding,
    VkDescriptorBindingFlags flags
){
    auto name_it = descriptor_names.insert(std::string(name)).first;
    auto it = named_bindings.find(*name_it);
    RB_CHECK(
        it != named_bindings.end() && it->second.binding != binding.binding,
        "Binding ", name, " has conflicting binding indices: ",
        it->second.binding, " and ", binding.binding, "."
    );

    named_bindings[*name_it] = {binding, flags};
    dirty = true;
}

void descriptor_set_layout::add(const shader_data& in_data, uint32_t target_set_index)
{
    shader_data data = in_data;
    gpu_pipeline::get_hooked_shader(data);
    if(data.bytes == 0) return;

    SpvReflectShaderModule rmod;
    SpvReflectResult result = spvReflectCreateShaderModule2(
        SPV_REFLECT_MODULE_FLAG_NO_COPY, data.bytes, data.data, &rmod
    );
    RB_CHECK(result != SPV_REFLECT_RESULT_SUCCESS, "SPIR-V reflection failed!");

    for(uint32_t i = 0; i < rmod.descriptor_binding_count; ++i)
    {
        SpvReflectDescriptorBinding& binding = rmod.descriptor_bindings[i];

        if(binding.set != target_set_index)
            continue;

        auto name_it = descriptor_names.insert(binding.name).first;
        auto it = named_bindings.find(binding.name);
        RB_CHECK(
            it != named_bindings.end() && it->second.binding != binding.binding,
            "Binding ", binding.name, " has conflicting binding indices: ",
            it->second.binding, " and ", binding.binding, "."
        );

        VkDescriptorSetLayoutBinding b;
        b.binding = binding.binding;
        b.descriptorType = (VkDescriptorType)binding.descriptor_type;
        b.descriptorCount = binding.count;
        b.stageFlags = rmod.shader_stage;
        b.pImmutableSamplers = nullptr;

        add(binding.name, b);
    }

    spvReflectDestroyShaderModule(&rmod);
}

void descriptor_set_layout::add(const raster_shader_data& data, uint32_t target_set_index)
{
    add(data.vertex, target_set_index);
    add(data.fragment, target_set_index);
}

void descriptor_set_layout::add(const ray_tracing_shader_data& data, uint32_t target_set_index)
{
    add(data.generation, target_set_index);
    for(const hit_group& hg: data.hit_groups)
    {
        add(hg.closest_hit, target_set_index);
        add(hg.any_hit, target_set_index);
        add(hg.intersection, target_set_index);
    }
    add(data.miss, target_set_index);
}

void descriptor_set_layout::set_binding_params(
    std::string_view name,
    uint32_t count,
    VkDescriptorBindingFlags flags
){
    auto it = named_bindings.find(name);
    if(it != named_bindings.end())
    {
        it->second.descriptorCount = count;
        it->second.flags = flags;
        dirty = true;
    }
}

descriptor_set::set_binding
descriptor_set_layout::find_binding(std::string_view name) const
{
    auto it = named_bindings.find(name);
    RB_CHECK(it == named_bindings.end(), "Missing binding ", name);

    return it->second;
}

VkDescriptorSetLayout descriptor_set_layout::get_layout() const
{
    refresh();
    return layout;
}

void descriptor_set_layout::refresh() const
{
    if(dirty)
    {
        bindings.clear();
        auto binding_flags = stack_allocate<VkDescriptorBindingFlags>(named_bindings.size());
        for(const auto& [name, binding]: named_bindings)
        {
            binding_flags[bindings.size()] = binding.flags;
            bindings.push_back(binding);
        }

        layout = create_descriptor_set_layout(
            *dev, bindings, binding_flags, push_descriptor_set
        );
        descriptor_pool_capacity = 0;

        dirty = false;
    }
}

descriptor_set::descriptor_set(device& dev)
: descriptor_set_layout(dev, false)
{
}

descriptor_set::~descriptor_set()
{
    reset(0);
}

void descriptor_set::reset(uint32_t count)
{
    refresh();

    if(named_bindings.size() == 0)
        return;

    for(VkDescriptorSet set: alternatives)
    {
        dev->gc.remove(set, [
            descriptor_set=set,
            pool=*pool,
            logical_device=dev->logical_device
        ](){
            vkFreeDescriptorSets(logical_device, pool, 1, &descriptor_set);
        });
    }
    alternatives.clear();

    if(descriptor_pool_capacity < count)
    {
        uint32_t safe_count = count * (dev->get_in_flight_count() + 2);
        auto pool_sizes = stack_allocate<VkDescriptorPoolSize>(bindings.size());

        size_t pool_count = calculate_descriptor_pool_sizes(
            bindings.size(), pool_sizes.get(), bindings.data(), safe_count
        );
        VkDescriptorPoolCreateInfo pool_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            nullptr,
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            (uint32_t)safe_count,
            (uint32_t)pool_count,
            pool_sizes.get()
        };
        VkDescriptorPool tmp_pool;
        vkCreateDescriptorPool(
            dev->logical_device,
            &pool_create_info,
            nullptr,
            &tmp_pool
        );
        pool = vkres(*dev, tmp_pool);
        descriptor_pool_capacity = count;
    }

    // Create descriptor sets
    if(count > 0)
    {
        auto layouts = stack_allocate<VkDescriptorSetLayout>(count);
        std::fill(layouts.begin(), layouts.end(), *layout);
        VkDescriptorSetAllocateInfo descriptor_alloc_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            pool,
            (uint32_t)count,
            layouts.get()
        };

        alternatives.resize(count);
        RB_CHECK(vkAllocateDescriptorSets(
            dev->logical_device, &descriptor_alloc_info, alternatives.data()
        ) != VK_SUCCESS, "Failed to allocate descriptor sets for some reason");

        for(VkDescriptorSet set: alternatives)
        {
            dev->gc.depend(*pool, set);
            dev->gc.depend(*layout, set);
        }
    }
}

void descriptor_set::set_image(
    uint32_t index,
    std::string_view name,
    argvec<VkImageView> views, argvec<VkSampler> samplers
){
    if(named_bindings.count(name) == 0 || views.size() == 0) return;

    set_binding bind = find_binding(name);
    auto image_infos = stack_allocate<VkDescriptorImageInfo>(views.size());

    RB_CHECK(
        !(bind.flags&VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT) &&
        views.size() != bind.descriptorCount,
        "Image view count does not match descriptor count, and "
        "VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT isn't set."
    );

    RB_CHECK(
        views.size() > bind.descriptorCount,
        "More images than descriptor allows!"
    );

    RB_CHECK(
        bind.descriptorType != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE &&
        bind.descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER &&
        bind.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        "Cannot set non-image descriptor as an image!"
    );

    for(size_t i = 0; i < views.size(); ++i)
    {
        VkSampler sampler = VK_NULL_HANDLE;
        if(samplers.size() == 1)
            sampler = samplers[0];
        else if(samplers.size() > 1)
        {
            assert(samplers.size() == views.size());
            sampler = samplers[i];
        }
        image_infos[i] = VkDescriptorImageInfo{
            sampler,
            views[i],
            bind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ?
                VK_IMAGE_LAYOUT_GENERAL :
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
    }
    VkWriteDescriptorSet write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        alternatives[index], (uint32_t)bind.binding, 0,
        (uint32_t)views.size(), bind.descriptorType,
        image_infos.get(), nullptr, nullptr
    };
    vkUpdateDescriptorSets(dev->logical_device, 1, &write, 0,  nullptr);

    dev->gc.depend_many(views.make_void_ptr().clip(), alternatives[index]);
    dev->gc.depend_many(samplers.make_void_ptr().clip(), alternatives[index]);
}

void descriptor_set::set_texture(
    uint32_t index,
    std::string_view name,
    const texture& tex,
    const sampler& s
){
    set_image(index, name, {tex.get_image_view()}, {s.get()});
}

void descriptor_set::set_image(
    uint32_t index,
    std::string_view name,
    const texture& tex
){
    set_image(index, name, argvec<VkImageView>{tex.get_image_view()});
}

void descriptor_set::set_buffer(
    uint32_t index,
    std::string_view name,
    argvec<VkBuffer> buffers,
    argvec<uint32_t> offsets
){
    if(named_bindings.count(name) == 0 || buffers.size() == 0) return;

    set_binding bind = find_binding(name);

    RB_CHECK(
        buffers.size() > bind.descriptorCount,
        "More buffers than descriptor allows!"
    );

    auto infos = stack_allocate<VkDescriptorBufferInfo>(buffers.size());
    for(size_t i = 0; i < buffers.size(); ++i)
    {
        infos[i] = {buffers[i], 0, VK_WHOLE_SIZE};
        if(i < offsets.size())
            infos[i].offset = offsets[i];
    }

    RB_CHECK(
        bind.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER &&
        bind.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        "Cannot set non-buffer descriptor as a buffer!"
    );

    for(size_t i = 0;;)
    {
        while(buffers[i] == VK_NULL_HANDLE && i < buffers.size()) ++i;
        uint32_t begin = i;
        while(buffers[i] != VK_NULL_HANDLE && i < buffers.size()) ++i;
        uint32_t end = i;

        if(end == begin)
            break;

        VkWriteDescriptorSet write = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
            alternatives[index], (uint32_t)bind.binding, (uint32_t)begin,
            end-begin, bind.descriptorType,
            nullptr, infos.get()+begin, nullptr
        };
        vkUpdateDescriptorSets(dev->logical_device, 1, &write, 0,  nullptr);
        dev->gc.depend_many(
            buffers.slice(begin, end-begin).make_void_ptr().clip(),
            alternatives[index]
        );
    }
}

void descriptor_set::set_acceleration_structure(
    uint32_t index,
    std::string_view name,
    VkAccelerationStructureKHR tlas
){
    if(named_bindings.count(name) == 0) return;

    set_binding bind = find_binding(name);

    RB_CHECK(
        bind.descriptorType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        "Cannot set non-acceleration structure descriptor as an acceleration "
        "structure!"
    );

    VkWriteDescriptorSetAccelerationStructureKHR as_write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        nullptr, 1, &tlas
    };

    VkWriteDescriptorSet write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, &as_write,
        alternatives[index], (uint32_t)bind.binding, 0,
        (uint32_t)1, bind.descriptorType,
        nullptr, nullptr, nullptr
    };
    vkUpdateDescriptorSets(dev->logical_device, 1, &write, 0,  nullptr);
    dev->gc.depend(tlas, alternatives[index]);
}

void descriptor_set::bind(
    VkCommandBuffer buf,
    VkPipelineLayout pipeline_layout,
    VkPipelineBindPoint bind_point,
    uint32_t alternative_index,
    uint32_t set_index
) const {
    if(alternatives.size() == 0)
        return;
    RB_CHECK(
        alternative_index >= alternatives.size(),
        "Alternative index is higher than number of alternatives"
    );
    vkCmdBindDescriptorSets(
        buf,
        bind_point,
        pipeline_layout,
        set_index, 1, &alternatives[alternative_index],
        0, nullptr
    );
    dev->gc.depend(alternatives[alternative_index], buf);
}

push_descriptor_set::push_descriptor_set(device& dev):
    descriptor_set_layout(dev, true), image_info_index(0), buffer_info_index(0),
    as_info_index(0)
{
}

push_descriptor_set::~push_descriptor_set()
{
}

void push_descriptor_set::set_image(
    std::string_view name,
    argvec<VkImageView> views,
    argvec<VkSampler> samplers
){
    if(named_bindings.count(name) == 0 || views.size() == 0) return;

    set_binding bind = find_binding(name);
    auto& image_infos = image_info_index < tmp_image_infos.size() ?
        tmp_image_infos[image_info_index] :
        tmp_image_infos.emplace_back(std::vector<VkDescriptorImageInfo>());
    image_info_index++;
    image_infos.resize(views.size());

    RB_CHECK(
        !(bind.flags&VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT) &&
        views.size() != bind.descriptorCount,
        "Image view count does not match descriptor count, and "
        "VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT isn't set."
    );

    RB_CHECK(
        views.size() > bind.descriptorCount,
        "More images than descriptor allows!"
    );

    RB_CHECK(
        bind.descriptorType != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE &&
        bind.descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER &&
        bind.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        "Cannot set non-image descriptor as an image!"
    );

    for(size_t i = 0; i < views.size(); ++i)
    {
        VkSampler sampler = VK_NULL_HANDLE;
        if(samplers.size() == 1)
            sampler = samplers[0];
        else if(samplers.size() > 1)
        {
            assert(samplers.size() == views.size());
            sampler = samplers[i];
        }
        image_infos[i] = VkDescriptorImageInfo{
            sampler,
            views[i],
            bind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ?
                VK_IMAGE_LAYOUT_GENERAL :
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        if(views[i] != VK_NULL_HANDLE)
            dependencies.push_back(views[i]);
        dependencies.push_back(sampler);
    }
    VkWriteDescriptorSet write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        VK_NULL_HANDLE, (uint32_t)bind.binding, 0,
        (uint32_t)views.size(), bind.descriptorType,
        image_infos.data(), nullptr, nullptr
    };
    writes.push_back(write);
}

void push_descriptor_set::set_texture(
    std::string_view name,
    const texture& tex,
    const sampler& s
){
    set_image(name, {tex.get_image_view()}, {s.get()});
}

void push_descriptor_set::set_image(
    std::string_view name,
    const texture& tex
){
    set_image(name, argvec<VkImageView>{tex.get_image_view()});
}

void push_descriptor_set::set_buffer(
    std::string_view name,
    argvec<VkBuffer> buffers,
    argvec<uint32_t> offsets
){
    if(named_bindings.count(name) == 0 || buffers.size() == 0) return;

    set_binding bind = find_binding(name);

    RB_CHECK(
        buffers.size() > bind.descriptorCount,
        "More buffers than descriptor allows!"
    );

    auto& infos = buffer_info_index < tmp_buffer_infos.size() ?
        tmp_buffer_infos[buffer_info_index] :
        tmp_buffer_infos.emplace_back(std::vector<VkDescriptorBufferInfo>());
    buffer_info_index++;
    infos.resize(buffers.size());

    for(size_t i = 0; i < buffers.size(); ++i)
    {
        infos[i] = {buffers[i], 0, VK_WHOLE_SIZE};
        if(i < offsets.size())
            infos[i].offset = offsets[i];
        if(buffers[i] != VK_NULL_HANDLE)
            dependencies.push_back(buffers[i]);
    }

    RB_CHECK(
        bind.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER &&
        bind.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        "Cannot set non-buffer descriptor as a buffer!"
    );

    for(size_t i = 0;;)
    {
        while(buffers[i] == VK_NULL_HANDLE && i < buffers.size()) ++i;
        uint32_t begin = i;
        while(buffers[i] != VK_NULL_HANDLE && i < buffers.size()) ++i;
        uint32_t end = i;

        if(end == begin)
            break;

        VkWriteDescriptorSet write = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
            VK_NULL_HANDLE, (uint32_t)bind.binding, (uint32_t)begin,
            end-begin, bind.descriptorType,
            nullptr, infos.data()+begin, nullptr
        };
        writes.push_back(write);
    }
}

void push_descriptor_set::set_acceleration_structure(
    std::string_view name,
    VkAccelerationStructureKHR tlas
){
    if(named_bindings.count(name) == 0) return;

    set_binding bind = find_binding(name);

    RB_CHECK(
        bind.descriptorType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        "Cannot set non-acceleration structure descriptor as an acceleration "
        "structure!"
    );

    if(tmp_as.size() < bindings.size())
    {
        tmp_as.resize(bindings.size());
        tmp_as_infos.resize(bindings.size());
    }

    VkWriteDescriptorSetAccelerationStructureKHR& as_write = tmp_as_infos[as_info_index];
    VkAccelerationStructureKHR& as = tmp_as[as_info_index];
    as_info_index++;
    as = tlas;
    as_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    as_write.pNext = nullptr;
    as_write.accelerationStructureCount = 1;
    as_write.pAccelerationStructures = &as;

    VkWriteDescriptorSet write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, &as_write,
        VK_NULL_HANDLE, (uint32_t)bind.binding, 0,
        (uint32_t)1, bind.descriptorType,
        nullptr, nullptr, nullptr
    };
    writes.push_back(write);
    dependencies.push_back(tlas);
}

void push_descriptor_set::push(
    VkCommandBuffer buf,
    VkPipelineLayout pipeline_layout,
    VkPipelineBindPoint bind_point,
    uint32_t set_index
){
    vkCmdPushDescriptorSetKHR(
        buf,
        bind_point,
        pipeline_layout,
        set_index,
        writes.size(),
        writes.data()
    );
    dev->gc.depend_many(dependencies, buf);
    image_info_index = 0;
    buffer_info_index = 0;
    as_info_index = 0;
    writes.clear();
    dependencies.clear();
}

}
