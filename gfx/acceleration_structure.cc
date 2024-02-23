#include "acceleration_structure.hh"
#include "mesh.hh"
#include "core/error.hh"
#include "scene_stage.hh"
#include "vulkan_helpers.hh"
#include "core/ecs.hh"
#include "model.hh"
#include "core/stack_allocator.hh"
#include "scene_stage.hh"
#include "point_light_tlas_instance.comp.h"

namespace
{
using namespace rb;
using namespace rb::gfx;

struct point_light_tlas_instance_push_constant_buffer
{
    uint32_t initial_index;
    uint32_t point_light_count;
    uint32_t mask;
    uint32_t flags;
    uint32_t sbt_offset;
    uint32_t as_reference[2];
};

}

namespace rb::gfx
{

bottom_level_acceleration_structure::bottom_level_acceleration_structure(
    device& dev,
    argvec<entry> primitives,
    bool dynamic
): async_loadable_resource(dev)
{
    super_impl_data* data = &impl(false);
    data->dynamic = dynamic;
    data->refreshes_since_last_rebuild = 0;
    data->geometry_count = primitives.size();

    std::vector<entry> src_primitives(primitives.begin(), primitives.end());
    std::vector<thread_pool::ticket> src_tickets;
    for(const entry& e: primitives)
    {
        const auto& tickets = e.prim->get_loading_tickets();
        src_tickets.insert(src_tickets.end(), tickets.begin(), tickets.end());
    }

    async_load([primitives = std::move(src_primitives), data](){
        refresh_internal(*data, primitives, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, true);
    }, src_tickets);
}

bottom_level_acceleration_structure::bottom_level_acceleration_structure(
    bottom_level_acceleration_structure* compatible
): async_loadable_resource(compatible->get_device())
{
    super_impl_data* data = &impl(false);
    super_impl_data* other_data = &compatible->impl(false);
    data->dynamic = other_data->dynamic;
    data->refreshes_since_last_rebuild = other_data->refreshes_since_last_rebuild;
    data->geometry_count = other_data->geometry_count;

    async_load([data, other_data](){
        copy_init_internal(*data, *other_data);
    }, compatible->get_loading_tickets());
}

bottom_level_acceleration_structure::bottom_level_acceleration_structure(
    device& dev, aabb box
): async_loadable_resource(dev)
{
    super_impl_data* data = &impl(false);
    data->dynamic = false;
    data->refreshes_since_last_rebuild = 0;
    data->geometry_count = 1;
    async_load([box, data](){
        refresh_internal(*data, box, VK_NULL_HANDLE, true);
    });
}

VkAccelerationStructureKHR bottom_level_acceleration_structure::get_handle() const
{
    return impl().as;
}

VkDeviceAddress bottom_level_acceleration_structure::get_device_address() const
{
    return impl().as_address;
}

void bottom_level_acceleration_structure::refresh(
    argvec<entry> primitives,
    VkCommandBuffer cmd,
    uint32_t frame_index,
    bool rebuild
){
    super_impl_data& d = impl();
    RB_CHECK(
        primitives.size() != d.geometry_count,
        "BLAS geometry count changed!"
    );
    refresh_internal(d, primitives, cmd, frame_index, VK_NULL_HANDLE, rebuild);
}

void bottom_level_acceleration_structure::refresh_from(
    argvec<entry> primitives,
    VkCommandBuffer cmd,
    uint32_t frame_index,
    bottom_level_acceleration_structure& src,
    bool rebuild
){
    super_impl_data& d = impl();
    RB_CHECK(
        primitives.size() != d.geometry_count,
        "BLAS geometry count changed!"
    );
    refresh_internal(d, primitives, cmd, frame_index, src.get_handle(), rebuild);
}

size_t bottom_level_acceleration_structure::get_refreshes_since_last_rebuild() const
{
    return impl(false).refreshes_since_last_rebuild;
}

void bottom_level_acceleration_structure::refresh_internal(
    super_impl_data& data,
    argvec<entry> primitives,
    VkCommandBuffer cmd,
    uint32_t frame_index,
    VkAccelerationStructureKHR src,
    bool rebuild
){
    auto geometries = stack_allocate<VkAccelerationStructureGeometryKHR>(primitives.size());
    auto ranges = stack_allocate<VkAccelerationStructureBuildRangeInfoKHR>(primitives.size());
    auto triangle_counts = stack_allocate<uint32_t>(primitives.size());

    size_t transform_count = 0;
    for(const entry& e: primitives)
        if(e.transform.has_value())
            transform_count++;

    if(transform_count != 0)
    {
        if(!data.transform_buffer.has_value())
        {
            data.transform_buffer.emplace(
                *data.dev,
                transform_count * sizeof(VkTransformMatrixKHR),
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
            );
            data.transform_address = data.transform_buffer->get_device_address();
        }
        else if(data.transform_buffer->resize(
            transform_count * sizeof(VkTransformMatrixKHR)
        )){
            data.transform_address = data.transform_buffer->get_device_address();
        }

        data.transform_buffer->update<uint8_t>(frame_index, [&](uint8_t* data)
        {
            size_t j = 0;
            for(size_t i = 0; i < primitives.size(); ++i)
            {
                if(primitives[i].transform.has_value())
                {
                    mat4 transform = transpose(primitives[i].transform.value());
                    memcpy(
                        data + sizeof(VkTransformMatrixKHR) * j,
                        (void*)&transform,
                        sizeof(VkTransformMatrixKHR)
                    );
                    ++j;
                }
            }
        });
    }

    size_t j = 0;
    for(size_t i = 0; i < primitives.size(); ++i)
    {
        const primitive* prim = primitives[i].prim;
        geometries[i] = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            nullptr,
            VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            VkAccelerationStructureGeometryTrianglesDataKHR{
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                nullptr,
                VK_FORMAT_R32G32B32_SFLOAT,
                prim->get_vertex_buffer_address(primitive::POSITION),
                sizeof(primitive::vertex_data::position[0]),
                (uint32_t)(prim->get_vertex_count()-1),
                VK_INDEX_TYPE_UINT32,
                prim->get_index_buffer_address(),
                primitives[i].transform.has_value() ?
                    data.transform_address + sizeof(VkTransformMatrixKHR) * j : 0
            },
            prim->is_opaque() ?
                VK_GEOMETRY_OPAQUE_BIT_KHR :
                VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR
        };
        if(primitives[i].transform.has_value())
            ++j;
        uint32_t triangle_count = prim->get_index_count()/3;
        ranges[i] = {triangle_count, 0, 0, 0};
        triangle_counts[i] = triangle_count;
    }

    refresh_internal_core(data, geometries, ranges, triangle_counts, cmd, frame_index, src, rebuild);
}

void bottom_level_acceleration_structure::refresh_internal(
    super_impl_data& data,
    aabb box,
    VkCommandBuffer cmd,
    bool rebuild
){
    VkAabbPositionsKHR aabb_data = {
        box.min.x, box.min.y, box.min.z,
        box.max.x, box.max.y, box.max.z
    };
    event e;
    data.aabb_buffer = upload_buffer(
        *data.dev, e,
        sizeof(VkAabbPositionsKHR),
        &aabb_data,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR|
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    );
    e.wait(*data.dev);

    VkBufferDeviceAddressInfo aabb_buffer_info = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        nullptr,
        data.aabb_buffer
    };

    VkDeviceOrHostAddressConstKHR aabb_data_address;
    aabb_data_address.deviceAddress = vkGetBufferDeviceAddress(
        data.dev->logical_device, &aabb_buffer_info
    );
    VkAccelerationStructureGeometryDataKHR geometry_data;
    geometry_data.aabbs = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR,
        nullptr,
        aabb_data_address,
        sizeof(VkAabbPositionsKHR)
    };
    VkAccelerationStructureGeometryKHR geometry = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        nullptr,
        VK_GEOMETRY_TYPE_AABBS_KHR,
        geometry_data,
        0
    };
    VkAccelerationStructureBuildRangeInfoKHR range = {1, 0, 0, 0};
    uint32_t primitive_counts = 1;

    refresh_internal_core(
        data, {&geometry, 1}, {&range, 1}, {&primitive_counts, 1}, cmd, 0, VK_NULL_HANDLE, rebuild
    );
    data.dev->gc.depend(*data.aabb_buffer, *data.as);
}

void bottom_level_acceleration_structure::copy_init_internal(
    super_impl_data& data,
    super_impl_data& other_data
){
    vkres<VkCommandBuffer> cmd = begin_command_buffer(*data.dev);

    data.acceleration_structure_size = other_data.acceleration_structure_size;
    data.scratch_size = other_data.scratch_size;

    uint32_t alignment = data.dev->as_properties.minAccelerationStructureScratchOffsetAlignment;
    data.as_scratch = create_gpu_buffer(
        *data.dev,
        data.scratch_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        alignment
    );

    VkBufferDeviceAddressInfo scratch_info = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        nullptr,
        data.as_scratch
    };

    data.as_scratch_address = vkGetBufferDeviceAddress(
        data.dev->logical_device,
        &scratch_info
    );

    data.as_buffer = create_gpu_buffer(
        *data.dev,
        data.acceleration_structure_size,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR|
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    );

    VkAccelerationStructureCreateInfoKHR as_create_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        nullptr,
        0,
        data.as_buffer,
        0,
        data.acceleration_structure_size,
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        0
    };

    VkAccelerationStructureKHR as_tmp;
    vkCreateAccelerationStructureKHR(
        data.dev->logical_device,
        &as_create_info,
        nullptr,
        &as_tmp
    );

    data.as = vkres(*data.dev, as_tmp);

    data.dev->gc.depend(*data.as_buffer, *data.as);
    data.dev->gc.depend(*data.as_scratch, *data.as);

    VkAccelerationStructureDeviceAddressInfoKHR as_addr_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        nullptr,
        *data.as
    };
    data.as_address = vkGetAccelerationStructureDeviceAddressKHR(
        data.dev->logical_device,
        &as_addr_info
    );

    VkCopyAccelerationStructureInfoKHR copy_info = {
        VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
        nullptr,
        *other_data.as,
        *data.as,
        VK_COPY_ACCELERATION_STRUCTURE_MODE_CLONE_KHR
    };
    vkCmdCopyAccelerationStructureKHR(cmd, &copy_info);

    data.dev->gc.depend(*data.as, *cmd);
    data.dev->gc.depend(*other_data.as, *cmd);

    data.loading_events.emplace_back(end_command_buffer(*data.dev, cmd));
}

void bottom_level_acceleration_structure::refresh_internal_core(
    super_impl_data& data,
    argvec<VkAccelerationStructureGeometryKHR> geometries,
    argvec<VkAccelerationStructureBuildRangeInfoKHR> ranges,
    argvec<uint32_t> max_primitive_counts,
    VkCommandBuffer cmd,
    uint32_t frame_index,
    VkAccelerationStructureKHR src,
    bool rebuild
){
    RB_CHECK(
        !data.dynamic && !rebuild,
        "Attempting update on a static BLAS! This is extremely slow!"
    );
    if(!data.dynamic) rebuild = true;

    vkres<VkCommandBuffer> cmd_local;
    if(cmd == VK_NULL_HANDLE)
    {
        cmd_local = begin_command_buffer(*data.dev);
        cmd = cmd_local;
    }
    constexpr VkBuildAccelerationStructureFlagsKHR dynamic_flags =
        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR|
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;

    constexpr VkBuildAccelerationStructureFlagsKHR static_flags =
        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR|
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR as_build_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        nullptr,
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        data.dynamic ? dynamic_flags : static_flags,
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        (uint32_t)geometries.size(),
        geometries.data(),
        nullptr,
        0
    };

    if(data.transform_buffer.has_value())
        data.transform_buffer->upload_individual(cmd, frame_index);

    VkAccelerationStructureCreateInfoKHR as_create_info;
    if(!data.as || !data.dynamic)
    {
        VkAccelerationStructureBuildSizesInfoKHR build_size = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
            nullptr
        };
        vkGetAccelerationStructureBuildSizesKHR(
            data.dev->logical_device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &as_build_info,
            max_primitive_counts.begin(),
            &build_size
        );
        data.acceleration_structure_size = build_size.accelerationStructureSize;
        data.scratch_size = build_size.buildScratchSize;
        if(data.dynamic)
            data.scratch_size = max(data.scratch_size, build_size.updateScratchSize);

        uint32_t alignment = data.dev->as_properties.minAccelerationStructureScratchOffsetAlignment;
        data.as_scratch = create_gpu_buffer(
            *data.dev,
            data.scratch_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            alignment
        );

        VkBufferDeviceAddressInfo scratch_info = {
            VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            nullptr,
            data.as_scratch
        };

        data.as_scratch_address = vkGetBufferDeviceAddress(
            data.dev->logical_device,
            &scratch_info
        );

        data.as_buffer = create_gpu_buffer(
            *data.dev,
            build_size.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR|
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        );

        as_create_info = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            nullptr,
            0,
            data.as_buffer,
            0,
            build_size.accelerationStructureSize,
            VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            0
        };

        VkAccelerationStructureKHR as_tmp;
        vkCreateAccelerationStructureKHR(
            data.dev->logical_device,
            &as_create_info,
            nullptr,
            &as_tmp
        );
        data.as = vkres(*data.dev, as_tmp);

        data.dev->gc.depend(*data.as_buffer, *data.as);
        data.dev->gc.depend(*data.as_scratch, *data.as);
    }

    vkres<VkQueryPool> compaction_query_pool;
    if(!data.dynamic && cmd_local)
    {
        VkQueryPoolCreateInfo info = {
            VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            nullptr, 0,
            VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            1, 0
        };
        VkQueryPool tmp_pool = VK_NULL_HANDLE;
        vkCreateQueryPool(data.dev->logical_device, &info, nullptr, &tmp_pool);
        compaction_query_pool = vkres(*data.dev, tmp_pool);
    }

    as_build_info.mode = rebuild ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
    as_build_info.srcAccelerationStructure = rebuild ? VK_NULL_HANDLE : (src ? src : *data.as);
    as_build_info.dstAccelerationStructure = *data.as;
    as_build_info.scratchData.deviceAddress = data.as_scratch_address;

    const VkAccelerationStructureBuildRangeInfoKHR* range_ptr = ranges.data();
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &as_build_info, &range_ptr);

    if(!rebuild && src) data.dev->gc.depend(src, cmd);
    data.dev->gc.depend(*data.as, cmd);

    if(data.dynamic && cmd_local)
    {
        data.loading_events.emplace_back(end_command_buffer(*data.dev, cmd));
    }
    else if(!data.dynamic && cmd_local)
    { // Compact static meshes!
        VkMemoryBarrier2KHR barrier = {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
            nullptr,
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
        };
        VkDependencyInfoKHR deps = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, nullptr, 0,
            1, &barrier, 0, nullptr, 0, nullptr
        };
        vkCmdPipelineBarrier2KHR(cmd, &deps);

        data.dev->gc.depend(*compaction_query_pool, cmd);
        vkCmdResetQueryPool(cmd, *compaction_query_pool, 0, 1);

        vkCmdWriteAccelerationStructuresPropertiesKHR(
            cmd,
            1,
            &*data.as,
            VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            *compaction_query_pool,
            0
        );

        end_command_buffer(*data.dev, cmd).wait(*data.dev);

        VkDeviceSize compacted_size = 0;
        vkGetQueryPoolResults(
            data.dev->logical_device,
            *compaction_query_pool,
            0, 1, sizeof(VkDeviceSize),
            &compacted_size, sizeof(VkDeviceSize),
            VK_QUERY_RESULT_WAIT_BIT
        );

        vkres<VkAccelerationStructureKHR> fat_as = std::move(data.as);
        vkres<VkBuffer> fat_as_buffer(std::move(data.as_buffer));

        data.acceleration_structure_size = compacted_size;
        data.as_buffer = create_gpu_buffer(
            *data.dev,
            compacted_size,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR|
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        );

        as_create_info.buffer = data.as_buffer;
        as_create_info.size = compacted_size;

        VkAccelerationStructureKHR as_tmp;
        vkCreateAccelerationStructureKHR(
            data.dev->logical_device,
            &as_create_info,
            nullptr,
            &as_tmp
        );
        data.as = vkres(*data.dev, as_tmp);

        vkres<VkCommandBuffer> compact_cmd = begin_command_buffer(*data.dev);
        VkCopyAccelerationStructureInfoKHR copy_info = {
            VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
            nullptr,
            *fat_as,
            *data.as,
            VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
        };
        vkCmdCopyAccelerationStructureKHR(compact_cmd, &copy_info);
        data.dev->gc.depend(*fat_as, compact_cmd);
        data.dev->gc.depend(*data.as, compact_cmd);
        data.dev->gc.depend(*data.as_buffer, *data.as);
        data.loading_events.emplace_back(end_command_buffer(*data.dev, compact_cmd));

        fat_as.reset();
        fat_as_buffer.reset();
        data.as_scratch.reset();
    }

    if(cmd_local)
    {
        VkAccelerationStructureDeviceAddressInfoKHR as_addr_info = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            nullptr,
            *data.as
        };
        data.as_address = vkGetAccelerationStructureDeviceAddressKHR(
            data.dev->logical_device,
            &as_addr_info
        );
    }
}

top_level_acceleration_structure::top_level_acceleration_structure(
    device& dev,
    size_t capacity
): dev(&dev), geometry_capacity(capacity)
{
    geometry_count = 0;

    tlas_instances.emplace(
        dev,
        geometry_capacity * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT|
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        16
    );

    tlas_instances_address = tlas_instances->get_device_address();

    VkAccelerationStructureGeometryKHR as_geom = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        nullptr,
        VK_GEOMETRY_TYPE_INSTANCES_KHR,
        {},
        VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    as_geom.geometry.instances = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        nullptr,
        VK_FALSE,
        VkDeviceOrHostAddressConstKHR{tlas_instances_address}
    };

    VkAccelerationStructureBuildGeometryInfoKHR as_build_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        nullptr,
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        1,
        &as_geom,
        nullptr,
        0
    };

    VkAccelerationStructureBuildSizesInfoKHR build_size = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        nullptr
    };
    uint32_t max_primitive_count = geometry_capacity;
    vkGetAccelerationStructureBuildSizesKHR(
        dev.logical_device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &as_build_info,
        &max_primitive_count,
        &build_size
    );

    tlas_buffer = create_gpu_buffer(
        dev,
        build_size.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR|
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    );

    VkAccelerationStructureCreateInfoKHR as_create_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        nullptr,
        0,
        tlas_buffer,
        0,
        build_size.accelerationStructureSize,
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        0
    };

    VkAccelerationStructureKHR as_tmp;
    vkCreateAccelerationStructureKHR(dev.logical_device, &as_create_info, nullptr, &as_tmp);
    tlas = vkres(dev, as_tmp);
    uint32_t alignment = dev.as_properties.minAccelerationStructureScratchOffsetAlignment;
    tlas_scratch = create_gpu_buffer(
        dev,
        build_size.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        alignment
    );
    VkBufferDeviceAddressInfo scratch_info = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        nullptr,
        tlas_scratch
    };
    tlas_scratch_address = vkGetBufferDeviceAddress(
        dev.logical_device, &scratch_info
    );

    dev.gc.depend(*tlas_scratch, *tlas);
    dev.gc.depend(*tlas_buffer, *tlas);
    dev.gc.depend((VkBuffer)tlas_instances.value(), *tlas);
}

VkAccelerationStructureKHR top_level_acceleration_structure::get_handle() const
{
    return tlas;
}

gpu_buffer& top_level_acceleration_structure::get_instances_buffer()
{
    return tlas_instances.value();
}

void top_level_acceleration_structure::refresh(
    scene& s,
    size_t instance_count,
    VkCommandBuffer cmd,
    uint32_t frame_index
){
    geometry_count = instance_count;

    VkMemoryBarrier2KHR barrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
        nullptr,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
        VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
        VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
    };
    VkDependencyInfoKHR deps = {
        VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, nullptr, 0,
        1, &barrier, 0, nullptr, 0, nullptr
    };
    vkCmdPipelineBarrier2KHR(cmd, &deps);

    VkAccelerationStructureGeometryKHR as_geom = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        nullptr,
        VK_GEOMETRY_TYPE_INSTANCES_KHR,
        {},
        VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    as_geom.geometry.instances = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        nullptr,
        VK_FALSE,
        {}
    };
    as_geom.geometry.instances.data.deviceAddress = tlas_instances_address;

    VkAccelerationStructureBuildGeometryInfoKHR as_build_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        nullptr,
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        VK_NULL_HANDLE,
        tlas,
        1,
        &as_geom,
        nullptr,
        tlas_scratch_address
    };

    VkAccelerationStructureBuildRangeInfoKHR range = {
        (uint32_t)geometry_count, 0, 0, 0
    };
    VkAccelerationStructureBuildRangeInfoKHR* range_ptr = &range;

    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &as_build_info, &range_ptr);

    dev->gc.depend(*tlas, cmd);
}

void top_level_acceleration_structure::copy(
    top_level_acceleration_structure& other,
    VkCommandBuffer cmd
){
    RB_CHECK(
        other.geometry_capacity != geometry_capacity,
        "Attempting to copy between top level acceleration structures of different capacities!"
    );
    geometry_count = other.geometry_count;

    VkCopyAccelerationStructureInfoKHR copy_info = {
        VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
        nullptr,
        *other.tlas,
        *tlas,
        VK_COPY_ACCELERATION_STRUCTURE_MODE_CLONE_KHR
    };
    vkCmdCopyAccelerationStructureKHR(cmd, &copy_info);

    dev->gc.depend(*other.tlas, cmd);
}

acceleration_structure_manager::acceleration_structure_manager(
    device& dev,
    descriptor_set& scene_data_set,
    size_t capacity,
    const options& opt
):  dev(&dev),
    opt(opt),
    point_light_tlas_instance_pipeline(dev),
    tlas_instance_data_set(dev),
    has_previous_data(false),
    tlas(dev, capacity),
    point_light_blas(dev, aabb{vec3(-1), vec3(1)})
{
    dev.gc.depend(point_light_blas.get_handle(), tlas.get_handle());
    if(opt.keep_previous)
    {
        prev_tlas.emplace(dev, capacity);
        dev.gc.depend(point_light_blas.get_handle(), prev_tlas->get_handle());
    }

    // TLAS instance data
    tlas_instance_data_set.add("tlas_instances", {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr});
    tlas_instance_data_set.reset(1);
    tlas_instance_data_set.set_buffer(0, "tlas_instances", (VkBuffer)tlas.get_instances_buffer());

    point_light_tlas_instance_pipeline.init(
        point_light_tlas_instance_comp_shader_binary,
        sizeof(point_light_tlas_instance_push_constant_buffer),
        {tlas_instance_data_set.get_layout(), scene_data_set.get_layout()}
    );
}

void acceleration_structure_manager::update(
    VkCommandBuffer cmd,
    uint32_t frame_index,
    scene& current_scene,
    descriptor_set& scene_data_set,
    size_t point_light_count,
    void* vinstance_entries,
    size_t instance_count,
    argvec<mesh*> animated_meshes
){
    // Copy old TLAS state if legit to do so
    if(has_previous_data)
    {
        prev_tlas->copy(tlas, cmd);
        dev->gc.depend_many(
            argvec<VkAccelerationStructureKHR>(prev_blas_handles).make_void_ptr(),
            cmd
        );
        prev_blas_handles.clear();
    }

    scene_stage::render_entry* instance_entries =
        (scene_stage::render_entry*)vinstance_entries;
    select_blas_groups(current_scene, frame_index, instance_entries, instance_count);
    build_blas_groups(current_scene, instance_entries, instance_count);

    if(animated_meshes.size() != 0)
    {
        // Barrier for animation updates
        VkMemoryBarrier2KHR barrier = {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
            nullptr,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
            VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_ACCESS_2_MEMORY_READ_BIT_KHR
        };
        VkDependencyInfoKHR deps = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, nullptr, 0,
            1, &barrier, 0, nullptr, 0, nullptr
        };
        vkCmdPipelineBarrier2KHR(cmd, &deps);

        animate_blas_groups(current_scene, animated_meshes, cmd, frame_index);

        // Barrier for BLAS updates
        barrier = {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
            nullptr,
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
        };
        deps = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, nullptr, 0,
            1, &barrier, 0, nullptr, 0, nullptr
        };
        vkCmdPipelineBarrier2KHR(cmd, &deps);
    }

    gpu_buffer& tlas_instances = tlas.get_instances_buffer();
    size_t geometry_count = 0;
    size_t instance_index = 0;

    tlas_instances.update<uint8_t>(frame_index, [&](uint8_t* deviceptr) {
        VkAccelerationStructureInstanceKHR* base = (VkAccelerationStructureInstanceKHR*)deviceptr;

        for(size_t i = 0; i < instance_groups.size(); ++i)
        {
            instance_group& group = instance_groups[i];
            const blas_data& bd = blas.at(group.blas_id);
            VkAccelerationStructureInstanceKHR inst;

            entity id = instance_entries[instance_index].id;

            pmat4 transform = mat4(1.0f);
            inst.flags = 0;
            if(!group.static_transform)
            {
                transformable* t = current_scene.get<transformable>(id);
                if(t) transform = transpose(t->get_global_transform());
            }
            ray_tracing_params* rt = current_scene.get<ray_tracing_params>(id);

            memcpy(&inst.transform, &transform, sizeof(inst.transform));
            inst.instanceCustomIndex = instance_index;
            inst.mask = group.static_transform ? TLAS_STATIC_MESH_MASK : TLAS_DYNAMIC_MESH_MASK;
            inst.instanceShaderBindingTableRecordOffset = 0;

            const bottom_level_acceleration_structure* current_blas = 
                bd.current_blas_is_alternate ? 
                &bd.alternate_blas.value() :
                &bd.primary_blas;

            inst.accelerationStructureReference = current_blas->get_device_address();
            instance_index += group.instance_count;
            dev->gc.depend(current_blas->get_handle(), cmd);
            if(opt.keep_previous)
                prev_blas_handles.push_back(current_blas->get_handle());

            if(!opt.force_allow_face_culling && (!rt || !rt->face_culling))
                inst.flags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            if(rt && rt->force_opaque)
                inst.flags |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;

            memcpy(&base[geometry_count], &inst, sizeof(VkAccelerationStructureInstanceKHR));

            geometry_count++;
        }
    });

    // TODO: Would be better to have with the shared barrier, maybe.
    tlas_instances.upload_individual(cmd, frame_index);

    if(point_light_count != 0)
    {
        point_light_tlas_instance_pipeline.bind(cmd);
        point_light_tlas_instance_pipeline.set_descriptors(cmd, tlas_instance_data_set, 0, 0);
        point_light_tlas_instance_pipeline.set_descriptors(cmd, scene_data_set, 0, 1);

        point_light_tlas_instance_push_constant_buffer pc;
        pc.initial_index = geometry_count;
        pc.point_light_count = point_light_count;
        pc.mask = TLAS_LIGHT_MASK;
        pc.flags = 0;
        pc.sbt_offset = 1;

        VkDeviceAddress addr = point_light_blas.get_device_address();
        memcpy(pc.as_reference, &addr, sizeof(addr));

        point_light_tlas_instance_pipeline.push_constants(cmd, &pc);
        point_light_tlas_instance_pipeline.dispatch(
            cmd, uvec3((point_light_count+127u)/128u, 1, 1)
        );

        geometry_count += point_light_count;

        // Barrier for point light instance upload
        buffer_barrier(cmd, (VkBuffer)tlas.get_instances_buffer());
    }

    tlas.refresh(current_scene, geometry_count, cmd, frame_index);

    if(!has_previous_data && opt.keep_previous)
    {
        VkMemoryBarrier2KHR barrier = {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
            nullptr,
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR,
            VK_ACCESS_2_MEMORY_READ_BIT_KHR|
            VK_ACCESS_2_MEMORY_WRITE_BIT_KHR
        };
        VkDependencyInfoKHR deps = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, nullptr, 0,
            1, &barrier, 0, nullptr, 0, nullptr
        };
        vkCmdPipelineBarrier2KHR(cmd, &deps);

        prev_tlas->copy(tlas, cmd);
        has_previous_data = true;
    }
}

void acceleration_structure_manager::clear_unused()
{
    uint64_t frame = dev->get_frame_counter();
    if(frame > 0 && opt.keep_previous)
        frame--;

    for(auto it = blas.begin(); it != blas.end();)
    {
        if(it->second.last_use_frame < frame)
            it = blas.erase(it);
        else ++it;
    }
}

void acceleration_structure_manager::clear_previous()
{
    has_previous_data = false;
    // TODO rest of it
}

VkAccelerationStructureKHR acceleration_structure_manager::get_tlas() const
{
    return tlas.get_handle();
}

VkAccelerationStructureKHR acceleration_structure_manager::get_prev_tlas() const
{
    return prev_tlas.has_value() ? prev_tlas->get_handle() : tlas.get_handle();
}

void acceleration_structure_manager::select_blas_groups(
    scene& s,
    uint32_t frame_index,
    void* vinstance_entries,
    size_t instance_count
){
    scene_stage::render_entry* instance_entries =
        (scene_stage::render_entry*)vinstance_entries;
    instance_groups.clear();

    entity last_entity_id = INVALID_ENTITY;
    for(size_t i = 0; i < instance_count; ++i)
    {
        scene_stage::render_entry& entry = instance_entries[i];
        model* mod = s.get<model>(entry.id);
        mesh* m = mod->m;
        transformable* t = s.get<transformable>(entry.id);
        bool static_mesh = !m->is_animated();
        bool static_transform = t->is_static();
        const primitive* p = (*m)[entry.vertex_group_index].get_primitive();
        uint64_t prim_id = p->get_unique_id();

        switch(opt.strategy)
        {
        case blas_strategy::PER_MESH:
            if(last_entity_id == entry.id)
            { // Instance for same object.
                instance_group& group = instance_groups.back();
                // If there are multiple instances of the same mesh, they will
                // end up with the same blas id because they consist of the
                // same primitives!
                hash_combine(group.blas_id, prim_id);
                group.instance_count++;
            }
            else instance_groups.push_back({prim_id, 1, false, static_mesh});
            break;

        case blas_strategy::MERGE_STATIC:
            if(instance_groups.size() == 0)
            {
                bool is_static = static_mesh && static_transform;
                instance_groups.push_back({prim_id, 1, is_static, static_mesh});
            }
            else
            {
                instance_group& group = instance_groups.back();
                if(
                    last_entity_id == entry.id || (
                    group.static_transform && group.static_mesh &&
                    static_transform && static_mesh)
                ){ // Merge into previous (either static pile or instance for same object)
                    hash_combine(group.blas_id, prim_id);
                    group.instance_count++;
                }
                else instance_groups.push_back({prim_id, 1, false, static_mesh});
            }
            break;
        }

        last_entity_id = entry.id;
    }
}

void acceleration_structure_manager::build_blas_groups(
    scene& s,
    void* vinstance_entries,
    size_t instance_count
){
    scene_stage::render_entry* instance_entries =
        (scene_stage::render_entry*)vinstance_entries;

    uint64_t frame = dev->get_frame_counter();

    std::vector<bottom_level_acceleration_structure::entry> entries;
    size_t instance_index = 0;
    // Add missing acceleration structures
    for(const instance_group& group: instance_groups)
    {
        auto it = blas.find(group.blas_id);
        if(it != blas.end())
        {
            it->second.last_use_frame = frame;
            instance_index += group.instance_count;
            continue;
        }

        entries.clear();
        bool double_sided = false;
        for(size_t i = 0; i < group.instance_count; ++i, ++instance_index)
        {
            const scene_stage::render_entry& e = instance_entries[instance_index];
            model* mod = s.get<model>(e.id);
            transformable* t = s.get<transformable>(e.id);
            mesh* m = mod->m;
            const primitive* p = (*m)[e.vertex_group_index].get_primitive();

            if(mod->materials[e.vertex_group_index].double_sided)
                double_sided = true;
            entries.push_back({
                p, group.static_transform ?
                    std::optional<mat4>(t->get_global_transform()) :
                    std::optional<mat4>()
            });
        }
        blas.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(group.blas_id),
            std::forward_as_tuple(
                frame, *dev,
                argvec<bottom_level_acceleration_structure::entry>(entries),
                !group.static_mesh,
                !group.static_mesh && opt.keep_previous
            )
        );
    }

    if(!opt.lazy_removal)
        clear_unused();
}

void acceleration_structure_manager::animate_blas_groups(
    scene& s,
    argvec<mesh*> animated_meshes,
    VkCommandBuffer cmd,
    uint32_t frame_index
){
    std::vector<bottom_level_acceleration_structure::entry> entries;
    for(mesh* m: animated_meshes)
    {
        uint64_t blas_id = 0;
        entries.clear();
        bool first = true;
        for(const vertex_group& vg: *m)
        {
            uint64_t id = vg.get_primitive()->get_unique_id();
            if(first) blas_id = id;
            else hash_combine(blas_id, id);
            first = false;
            entries.push_back({vg.get_primitive(), {}});
        }

        auto it = blas.find(blas_id);
        if(it == blas.end())
            RB_PANIC("Missing BLAS for animated mesh!");

        // TODO: Periodic, alternating rebuilds?
        if(it->second.alternate_blas.has_value())
        {
            it->second.current_blas_is_alternate = !it->second.current_blas_is_alternate;
            bottom_level_acceleration_structure* cur_blas =
                it->second.current_blas_is_alternate ? 
                &it->second.alternate_blas.value() :
                &it->second.primary_blas;
            bottom_level_acceleration_structure* prev_blas =
                it->second.current_blas_is_alternate ? 
                &it->second.primary_blas :
                &it->second.alternate_blas.value();
            cur_blas->refresh_from(
                entries,
                cmd,
                frame_index,
                *prev_blas,
                false
            );
        }
        else
        {
            it->second.primary_blas.refresh(
                entries,
                cmd,
                frame_index,
                false
            );
        }
    }
}

acceleration_structure_manager::blas_data::blas_data(
    size_t last_use_frame,
    device& dev,
    argvec<bottom_level_acceleration_structure::entry> primitives,
    bool dynamic,
    bool create_alternate
): last_use_frame(last_use_frame), primary_blas(dev, primitives, dynamic)
{
    if(create_alternate)
        alternate_blas.emplace(&primary_blas);
}

}
