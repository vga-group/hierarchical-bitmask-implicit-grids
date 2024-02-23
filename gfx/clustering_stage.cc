#include "clustering_stage.hh"
#include "vulkan_helpers.hh"
#include "light.hh"
#include "gpu_pipeline.hh"
#include "light_ranges.comp.h"
#include "light_morton.comp.h"
#include "clustering.comp.h"
#include "clustering_hierarchy.comp.h"
#include "decal_order.comp.h"
#include "decal_ranges.comp.h"

#define MORTON_BITS_PER_AXIS 8
#define CLUSTER_AXIS_COUNT 3
#define CLUSTER_HIERARCHY_THRESHOLD 1024

namespace
{
using namespace rb;
using namespace rb::gfx;

struct range_push_constant_buffer
{
    uint32_t range_axis_offset;
    uint32_t item_count;
    uint32_t axis_index;
    uint32_t use_metadata;
    float cluster_slice;
    float cluster_min;
    float visibility_bias;
};

struct clustering_push_constant_buffer
{
    uint32_t range_axis_offset;
    uint32_t cluster_axis_offset;
    uint32_t cluster_size;
    uint32_t item_count;
    uint32_t axis;
};

struct hierarchy_push_constant_buffer
{
    uint32_t cluster_axis_offset;
    uint32_t hierarchy_axis_offset;
    uint32_t cluster_slice_size;
    uint32_t hierarchy_slice_size;
    uint32_t axis;
};

struct order_push_constant_buffer
{
    pvec4 bounds_min;
    pvec4 bounds_step;
    uint32_t item_count;
    uint32_t morton_shift;
    uint32_t morton_bits;
};

uint32_t get_cluster_slice_size(uint32_t max_items, uint32_t hierarchy_level = 0)
{
    if(hierarchy_level == 0)
        return sizeof(uvec4)*((max_items+127)/128);
    else
    {
        uint32_t full_size = get_cluster_slice_size(max_items, hierarchy_level-1) / sizeof(uvec4);
        return sizeof(uvec4)*((full_size+127)/128);
    }
}

void get_cluster_axis_buffer_offsets(
    uint32_t max_items,
    uint32_t cluster_resolution,
    uint32_t hierarchy_levels,
    uint32_t* output
){
    uint32_t offset = 0;
    for(uint32_t i = 0; i < hierarchy_levels; ++i)
    {
        uint32_t light_bitmask_size = get_cluster_slice_size(max_items, i);

        for(uint32_t axis = 0; axis < CLUSTER_AXIS_COUNT; ++axis)
        {
            *output = offset;
            output++;
            offset += light_bitmask_size * cluster_resolution;
        }
        output++;
    }
}

uint32_t get_total_slice_count(uint32_t resolution, uint32_t scale = 1)
{
    return (resolution / scale) * CLUSTER_AXIS_COUNT;
}

void run_clustering(
    VkCommandBuffer cmd,
    uint32_t item_count,
    size_t max_items,
    size_t item_size,
    size_t metadata_size,
    size_t cluster_resolution,
    uint32_t sort_bits,
    float visibility_bias,
    compute_pipeline& order_pipeline,
    compute_pipeline& range_pipeline,
    compute_pipeline& clustering_pipeline,
    compute_pipeline& hierarchy_pipeline,
    const descriptor_set& clustering_data_set,
    const descriptor_set& scene_data_set,
    uint32_t clustering_data_set_index,
    vec3 cluster_bounds[2],
    radix_sort* sorter,
    VkBuffer unsorted_items,
    VkBuffer sort_order,
    VkBuffer sorted_items,
    VkBuffer unsorted_metadata,
    VkBuffer sorted_metadata,
    VkBuffer sort_mapping,
    uint32_t frame_index,
    timer* sort_timer,
    timer* range_timer,
    timer* bitmask_timer,
    timer* hierarchy_timer
){
    if(item_count == 0)
        return;

    if(sort_bits > 0)
    {// Calculate sorting for all items.
        if(sort_timer) sort_timer->start(cmd, frame_index);
        order_pipeline.bind(cmd);
        order_pipeline.set_descriptors(cmd, clustering_data_set, clustering_data_set_index);
        order_push_constant_buffer pc;

        for(int i = 0; i < CLUSTER_AXIS_COUNT; ++i)
        {
            vec2 bounds = vec2(cluster_bounds[0][i], cluster_bounds[1][i]);
            pc.bounds_min[i] = bounds.x;
            pc.bounds_step[i] = (bounds.y - bounds.x) / float(1 << MORTON_BITS_PER_AXIS);
        }
        pc.item_count = item_count;
        pc.morton_shift = 32 - sort_bits;
        pc.morton_bits = MORTON_BITS_PER_AXIS;
        order_pipeline.push_constants(cmd, &pc);
        order_pipeline.dispatch(cmd, uvec3((item_count+63u)/64u, 1, 1));

        // Sort the items!
        sorter->sort(
            cmd, unsorted_items, sort_order, sorted_items,
            item_size, item_count, min(sort_bits, 32u)
        );

        // Sort metadata with the same order if needed.
        if(unsorted_metadata)
        {
            sorter->resort(cmd, unsorted_metadata, sorted_metadata, metadata_size, item_count);
        }

        if(sort_mapping)
            sorter->get_sort_index(cmd, sort_mapping, item_count);
        if(sort_timer) sort_timer->stop(cmd, frame_index);
    }

    {// Now, we calculate item ranges.
        if(range_timer) range_timer->start(cmd, frame_index);
        range_pipeline.bind(cmd);
        range_pipeline.set_descriptors(cmd, clustering_data_set, clustering_data_set_index, 0);
        range_pipeline.set_descriptors(cmd, scene_data_set, 0, 1);
        range_push_constant_buffer pc;
        pc.item_count = item_count;
        pc.use_metadata = unsorted_metadata != VK_NULL_HANDLE ? 1 : 0;
        for(int i = 0; i < CLUSTER_AXIS_COUNT; ++i)
        {
            pc.axis_index = i;
            pc.range_axis_offset = i * max_items;
            vec2 bounds = vec2(cluster_bounds[0][i], cluster_bounds[1][i]);
            pc.cluster_slice = (bounds.y - bounds.x) / cluster_resolution;
            pc.cluster_min = bounds.x;
            pc.visibility_bias = visibility_bias;
            range_pipeline.push_constants(cmd, &pc);
            range_pipeline.dispatch(
                cmd,
                uvec3((item_count+127u)/128u, 1, 1)
            );
        }
        if(range_timer) range_timer->stop(cmd, frame_index);
    }

    if(bitmask_timer) bitmask_timer->start(cmd, frame_index);
    // Barrier for cluster
    VkMemoryBarrier2KHR barrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
        nullptr,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT
    };
    VkDependencyInfoKHR deps = {
        VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, nullptr, 0,
        1, &barrier, 0, nullptr, 0, nullptr
    };
    vkCmdPipelineBarrier2KHR(cmd, &deps);

    {// Now, we calculate the high-resolution primary cluster.
        clustering_pipeline.bind(cmd);
        clustering_pipeline.set_descriptors(cmd, clustering_data_set, clustering_data_set_index, 0);

        uint32_t uvec4_slice_count = get_cluster_slice_size(max_items, 0)/sizeof(uvec4);

        clustering_push_constant_buffer pc;
        pc.item_count = item_count;

        for(int i = 0; i < CLUSTER_AXIS_COUNT; ++i)
        {
            pc.axis = i;
            pc.range_axis_offset = i * max_items;
            pc.cluster_axis_offset = i * uvec4_slice_count * 4 * cluster_resolution;
            pc.cluster_size = cluster_resolution;
            clustering_pipeline.push_constants(cmd, &pc);
            clustering_pipeline.dispatch(cmd, uvec3((cluster_resolution + 31u)/32u, uvec4_slice_count, 1));
        }
    }
    if(bitmask_timer) bitmask_timer->stop(cmd, frame_index);

    if(max_items >= CLUSTER_HIERARCHY_THRESHOLD)
    { // Finally, we build the hierarchical bitmask from the exact cluster.
        if(hierarchy_timer) hierarchy_timer->start(cmd, frame_index);
        hierarchy_pipeline.bind(cmd);
        hierarchy_pipeline.set_descriptors(cmd, clustering_data_set, clustering_data_set_index);

        hierarchy_push_constant_buffer pc;
        pc.cluster_slice_size = get_cluster_slice_size(max_items, 0)/sizeof(uvec4);
        pc.hierarchy_slice_size = get_cluster_slice_size(max_items, 1)/sizeof(uint32_t);

        for(int i = 0; i < CLUSTER_AXIS_COUNT; ++i)
        {
            pc.axis = i;
            pc.cluster_axis_offset = i * pc.cluster_slice_size * cluster_resolution;
            pc.hierarchy_axis_offset = i * pc.hierarchy_slice_size * cluster_resolution;
            hierarchy_pipeline.push_constants(cmd, &pc);
            hierarchy_pipeline.dispatch(
                cmd,
                uvec3(
                    cluster_resolution,
                    (pc.hierarchy_slice_size+7u)/8u,
                    1
                )
            );
        }

        vkCmdPipelineBarrier2KHR(cmd, &deps);
        if(hierarchy_timer) hierarchy_timer->stop(cmd, frame_index);
    }
}

}

namespace rb::gfx
{

clustering_stage::clustering_stage(scene_stage& scene, const options& opt)
:   render_stage(scene.get_device()),
    opt(opt),
    scene_data(&scene),
    stage_timer(scene.get_device(), "light and decal clustering"),
    sort_timer(scene.get_device(), "\tsort"),
    range_timer(scene.get_device(), "\trange"),
    bitmask_timer(scene.get_device(), "\tbitmask"),
    hierarchy_timer(scene.get_device(), "\thierarchy"),
    last_update_frame(UINT64_MAX),
    light_cluster_slices(create_gpu_buffer(
        scene.get_device(),
        get_cluster_slice_size(scene.opt.max_lights, 0) *
            get_total_slice_count(opt.light_cluster_resolution) +
        get_cluster_slice_size(scene.opt.max_lights, 1) *
            get_total_slice_count(opt.light_cluster_resolution),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    )),
    light_cluster_ranges(create_gpu_buffer(
        scene.get_device(),
        sizeof(uint32_t) * CLUSTER_AXIS_COUNT * scene.opt.max_lights,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    )),
    decal_cluster_slices(create_gpu_buffer(
        scene.get_device(),
        get_cluster_slice_size(scene.opt.max_decals, 0) *
            get_total_slice_count(opt.decal_cluster_resolution) +
        get_cluster_slice_size(scene.opt.max_decals, 1) *
            get_total_slice_count(opt.decal_cluster_resolution),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    )),
    decal_cluster_ranges(create_gpu_buffer(
        scene.get_device(),
        sizeof(uint32_t) * CLUSTER_AXIS_COUNT * scene.opt.max_decals,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    )),
    sorted_point_lights(create_gpu_buffer(
        scene.get_device(),
        scene.opt.max_lights >= CLUSTER_HIERARCHY_THRESHOLD ? scene.unsorted_point_lights.get_size() : 0,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    )),
    sorted_decals(create_gpu_buffer(scene.get_device(), scene.unsorted_decals.get_size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)),
    light_morton_pipeline(scene.get_device()),
    light_range_pipeline(scene.get_device()),
    clustering_pipeline(scene.get_device()),
    hierarchy_pipeline(scene.get_device()),
    decal_order_pipeline(scene.get_device()),
    decal_range_pipeline(scene.get_device()),
    clustering_data_set(scene.get_device())
{
    if(scene.opt.max_lights >= CLUSTER_HIERARCHY_THRESHOLD || scene.opt.max_decals > 0)
    { // Decals are always sorted due to their priority system.
        sorter.emplace(
            scene.get_device(),
            max(scene.opt.max_lights >= CLUSTER_HIERARCHY_THRESHOLD ? scene.opt.max_lights : 0, scene.opt.max_decals)
        );
        sort_order = sorter->create_keyval_buffer();
    }

    clustering_data_set.add("cluster_ranges", {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    clustering_data_set.add("cluster_slices", {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    clustering_data_set.add("hierarchy_slices", {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    clustering_data_set.add("sorting_order", {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    clustering_data_set.add("items", {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    clustering_data_set.add(
        "metadata", {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr},
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    );
    clustering_data_set.reset(3);

    // Light clustering data
    for(int i = 0; i < 2; ++i)
    { // 2 due to metadata having ping-pong.
        clustering_data_set.set_buffer(i, "cluster_ranges", (VkBuffer)light_cluster_ranges);
        clustering_data_set.set_buffer(i, "cluster_slices", (VkBuffer)light_cluster_slices);
        clustering_data_set.set_buffer(i, "hierarchy_slices", (VkBuffer)light_cluster_slices,
            CLUSTER_AXIS_COUNT * get_cluster_slice_size(scene.opt.max_lights, 0) * opt.light_cluster_resolution
        );
        clustering_data_set.set_buffer(i, "sorting_order", (VkBuffer)sort_order);
        clustering_data_set.set_buffer(i, "items", (VkBuffer)scene.unsorted_point_lights);
    }

    // Decal clustering data
    clustering_data_set.set_buffer(2, "cluster_ranges", (VkBuffer)decal_cluster_ranges);
    clustering_data_set.set_buffer(2, "cluster_slices", (VkBuffer)decal_cluster_slices);
    clustering_data_set.set_buffer(2, "hierarchy_slices", (VkBuffer)decal_cluster_slices,
        CLUSTER_AXIS_COUNT * get_cluster_slice_size(scene.opt.max_decals, 0) * opt.decal_cluster_resolution
    );
    clustering_data_set.set_buffer(2, "sorting_order", (VkBuffer)sort_order);
    clustering_data_set.set_buffer(2, "items", (VkBuffer)scene.decal_metadata);

    light_morton_pipeline.init(
        light_morton_comp_shader_binary,
        sizeof(order_push_constant_buffer),
        {clustering_data_set.get_layout()}
    );

    light_range_pipeline.init(
        light_ranges_comp_shader_binary,
        sizeof(range_push_constant_buffer),
        {clustering_data_set.get_layout(), scene.get_descriptor_set().get_layout()}
    );

    clustering_pipeline.init(
        clustering_comp_shader_binary,
        sizeof(clustering_push_constant_buffer),
        {clustering_data_set.get_layout()}
    );

    hierarchy_pipeline.init(
        clustering_hierarchy_comp_shader_binary,
        sizeof(hierarchy_push_constant_buffer),
        {clustering_data_set.get_layout()}
    );

    decal_order_pipeline.init(
        decal_order_comp_shader_binary,
        sizeof(order_push_constant_buffer),
        {clustering_data_set.get_layout()}
    );

    decal_range_pipeline.init(
        decal_ranges_comp_shader_binary,
        sizeof(range_push_constant_buffer),
        {clustering_data_set.get_layout(), scene.get_descriptor_set().get_layout()}
    );

    scene.cluster_provider = this;
}

clustering_stage::~clustering_stage()
{
    scene_data->cluster_provider = nullptr;
}

void clustering_stage::get_specialization_info(specialization_info& info) const
{
    scene_data->get_specialization_info(info);
    info[1] = uint32_t(get_cluster_slice_size(scene_data->opt.max_lights, 0)/sizeof(uvec4));
    info[2] = scene_data->opt.max_lights >= CLUSTER_HIERARCHY_THRESHOLD ?
        uint32_t(get_cluster_slice_size(scene_data->opt.max_lights, 1)/sizeof(uvec4)) : 0u;
    info[3] = uint32_t(get_cluster_slice_size(scene_data->opt.max_decals, 0)/sizeof(uvec4));
    info[4] = scene_data->opt.max_decals >= CLUSTER_HIERARCHY_THRESHOLD ?
        uint32_t(get_cluster_slice_size(scene_data->opt.max_decals, 1)/sizeof(uvec4)) : 0u;
}

void clustering_stage::get_light_cluster_axis_buffer_offsets(uint32_t* output) const
{
    get_cluster_axis_buffer_offsets(scene_data->opt.max_lights, opt.light_cluster_resolution, 2, output);
}

void clustering_stage::get_decal_cluster_axis_buffer_offsets(uint32_t* output) const
{
    get_cluster_axis_buffer_offsets(scene_data->opt.max_decals, opt.decal_cluster_resolution, 2, output);
}

scene_stage* clustering_stage::get_scene_data() const
{
    return scene_data;
}

void clustering_stage::update_buffers(uint32_t frame_index)
{
    clear_commands();
    VkCommandBuffer cmd = compute_commands(true);
    stage_timer.start(cmd, frame_index);
    if(scene_data->current_scene)
    {
        run_light_clustering(cmd, frame_index);
        run_decal_clustering(cmd, frame_index);
    }
    stage_timer.stop(cmd, frame_index);
    use_compute_commands(cmd, frame_index);
    last_update_frame = dev->get_frame_counter();
}

void clustering_stage::run_light_clustering(VkCommandBuffer cmd, uint32_t frame_index)
{
    run_clustering(
        cmd,
        scene_data->point_light_count,
        scene_data->opt.max_lights,
        scene_data->unsorted_point_lights.get_size() / scene_data->opt.max_lights,
        0,
        opt.light_cluster_resolution,
        scene_data->opt.max_lights >= CLUSTER_HIERARCHY_THRESHOLD ? MORTON_BITS_PER_AXIS * CLUSTER_AXIS_COUNT : 0,
        0,
        light_morton_pipeline,
        light_range_pipeline,
        clustering_pipeline,
        hierarchy_pipeline,
        clustering_data_set,
        scene_data->get_descriptor_set(),
        frame_index & 1,
        scene_data->light_bounds,
        (sorter ? &sorter.value() : nullptr),
        scene_data->unsorted_point_lights,
        sort_order,
        sorted_point_lights,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        point_light_sort_indices[frame_index&1],
        frame_index,
        &sort_timer,
        &range_timer,
        &bitmask_timer,
        &hierarchy_timer
    );
}

void clustering_stage::run_decal_clustering(VkCommandBuffer cmd, uint32_t frame_index)
{
    run_clustering(
        cmd,
        scene_data->decal_count,
        scene_data->opt.max_decals,
        scene_data->unsorted_decals.get_size() / scene_data->opt.max_decals,
        0,
        opt.decal_cluster_resolution,
        8+MORTON_BITS_PER_AXIS * CLUSTER_AXIS_COUNT,
        0,
        decal_order_pipeline,
        decal_range_pipeline,
        clustering_pipeline,
        hierarchy_pipeline,
        clustering_data_set,
        scene_data->get_descriptor_set(),
        2,
        scene_data->decal_bounds,
        (sorter ? &sorter.value() : nullptr),
        scene_data->unsorted_decals,
        sort_order,
        sorted_decals,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        frame_index,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
}

}

