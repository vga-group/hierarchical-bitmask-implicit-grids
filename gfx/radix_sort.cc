#include "radix_sort.hh"
#include "core/error.hh"
#include "radix_sort/radix_sort_vk.h"
#include "vulkan_helpers.hh"
#include "sort_placement.comp.h"
#include "sort_order.comp.h"

namespace
{

struct placement_push_constant_buffer
{
    uint32_t entry_count;
    uint32_t payload_size;
};

struct order_push_constant_buffer
{
    uint32_t entry_count;
};

}

namespace rb::gfx
{

radix_sort::radix_sort(
    device& dev,
    size_t max_count,
    size_t keyval_dwords
):
    dev(&dev),
    rs_instance(nullptr),
    max_count(max_count),
    keyval_dwords(keyval_dwords),
    sort_set(dev),
    placement_pipeline(dev),
    order_set(dev),
    order_pipeline(dev)
{
    radix_sort_vk_target_t* rs_target = radix_sort_vk_target_auto_detect(
        &dev.physical_device_props.properties,
        &dev.subgroup_properties,
        keyval_dwords
    );

    rs_instance = radix_sort_vk_create(
        dev.logical_device,
        nullptr,
        VK_NULL_HANDLE,
        rs_target
    );
    free(rs_target);

    radix_sort_vk_memory_requirements_t memory_requirements;
    radix_sort_vk_get_memory_requirements(
        (const radix_sort_vk_t*)rs_instance, max_count, &memory_requirements
    );

    RB_CHECK(
        memory_requirements.keyval_size != keyval_dwords * 4,
        "Radix sort implementation does not obey requested keyval size!"
    );

    alloc_alignment = max(
        memory_requirements.internal_alignment,
        memory_requirements.keyvals_alignment
    );
    alloc_keyvals_size = (memory_requirements.keyvals_size + alloc_alignment-1)/alloc_alignment*alloc_alignment;
    alloc_internal_size = memory_requirements.internal_size;

    sort_set.add(
        "payload_in",
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr}
    );
    sort_set.add(
        "payload_out",
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr}
    );
    sort_set.add(
        "keyvals",
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr}
    );

    placement_pipeline.init(
        sort_placement_comp_shader_binary,
        sizeof(placement_push_constant_buffer),
        {sort_set.get_layout()}
    );

    shader_data order_s(sort_order_comp_shader_binary);
    order_set.add(order_s);

    order_pipeline.init(
        sort_order_comp_shader_binary,
        sizeof(order_push_constant_buffer),
        {order_set.get_layout()}
    );
}

radix_sort::~radix_sort()
{
    dev->gc.remove(
        rs_instance,
        [rs_instance = rs_instance, logical_device = dev->logical_device](){
            radix_sort_vk_destroy(
                (radix_sort_vk_t*)rs_instance,
                logical_device,
                nullptr
            );
        }
    );
}

vkres<VkBuffer> radix_sort::create_keyval_buffer() const
{
    return create_gpu_buffer(
        *dev,
        alloc_keyvals_size * 2 + alloc_internal_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT|
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        alloc_alignment
    );
}

void radix_sort::sort(
    VkCommandBuffer cmd,
    VkBuffer payload,
    VkBuffer keyvals,
    VkBuffer output,
    size_t payload_size,
    size_t count,
    size_t key_bits
){
    RB_CHECK(
        payload_size % sizeof(uvec4) != 0,
        "TODO: sorting data without 16-byte alignment"
    );

    VkMemoryBarrier2KHR barrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
        nullptr,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
        VK_ACCESS_2_MEMORY_WRITE_BIT_KHR | VK_ACCESS_2_MEMORY_READ_BIT_KHR
    };
    VkDependencyInfoKHR deps = {
        VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, nullptr, 0,
        1, &barrier, 0, nullptr, 0, nullptr
    };

    vkCmdPipelineBarrier2KHR(cmd, &deps);

    radix_sort_vk_sort_info_t sort_info;
    sort_info.ext = nullptr;
    sort_info.key_bits = key_bits;
    sort_info.count = count;
    sort_info.keyvals_even.buffer = keyvals;
    sort_info.keyvals_even.offset = 0;
    sort_info.keyvals_even.range = alloc_keyvals_size;
    sort_info.keyvals_odd.buffer = keyvals;
    sort_info.keyvals_odd.offset = alloc_keyvals_size;
    sort_info.keyvals_odd.range = alloc_keyvals_size;
    sort_info.internal.buffer = keyvals;
    sort_info.internal.offset = alloc_keyvals_size*2;
    sort_info.internal.range = alloc_internal_size;

    radix_sort_vk_sort(
        (const radix_sort_vk_t*)rs_instance,
        &sort_info,
        dev->logical_device,
        cmd,
        &keyvals_sorted
    );
    dev->gc.depend(rs_instance, cmd);

    vkCmdPipelineBarrier2KHR(cmd, &deps);

    sort_set.set_buffer("payload_in", payload);
    sort_set.set_buffer("payload_out", output);
    sort_set.set_buffer("keyvals", {keyvals_sorted.buffer}, {(uint32_t)keyvals_sorted.offset});

    placement_pipeline.bind(cmd);
    placement_pipeline.push_descriptors(cmd, sort_set, 0);
    placement_push_constant_buffer pc;
    pc.entry_count = count;
    pc.payload_size = payload_size/sizeof(uvec4);
    placement_pipeline.push_constants(cmd, &pc);
    placement_pipeline.dispatch(cmd, uvec3((count+63u)/64u, 1, 1));

    vkCmdPipelineBarrier2KHR(cmd, &deps);
}

void radix_sort::get_sort_index(
    VkCommandBuffer cmd,
    VkBuffer output,
    size_t count
){
    order_set.set_buffer("out_map", output);
    order_set.set_buffer("keyvals", {keyvals_sorted.buffer}, {(uint32_t)keyvals_sorted.offset});

    order_pipeline.bind(cmd);
    order_pipeline.push_descriptors(cmd, order_set, 0);
    order_push_constant_buffer pc;
    pc.entry_count = count;

    order_pipeline.push_constants(cmd, &pc);
    order_pipeline.dispatch(cmd, uvec3((count+63u)/64u, 1, 1));

    buffer_barrier(
        cmd, output,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR
    );
}

void radix_sort::resort(
    VkCommandBuffer cmd,
    VkBuffer payload,
    VkBuffer output,
    size_t payload_size,
    size_t count
){
    RB_CHECK(
        payload_size % sizeof(uvec4) != 0,
        "TODO: sorting data without 16-byte alignment"
    );

    sort_set.set_buffer("payload_in", payload);
    sort_set.set_buffer("payload_out", output);
    sort_set.set_buffer("keyvals", {keyvals_sorted.buffer}, {(uint32_t)keyvals_sorted.offset});

    placement_pipeline.bind(cmd);
    placement_pipeline.push_descriptors(cmd, sort_set, 0);
    placement_push_constant_buffer pc;
    pc.entry_count = count;
    pc.payload_size = payload_size/sizeof(uvec4);
    placement_pipeline.push_constants(cmd, &pc);
    placement_pipeline.dispatch(cmd, uvec3((count+63u)/64u, 1, 1));

    buffer_barrier(
        cmd, output,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR
    );
}

}
