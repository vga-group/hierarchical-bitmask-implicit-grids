#include "vulkan_helpers.hh"
#include "context.hh"
#include "core/error.hh"
#include "core/stack_allocator.hh"
#include "stb_image.h"
// Forgive me...
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_format_traits.hpp"
#include "stb_image_write.h"
#ifdef RAYBASE_HAS_SDL2
#include <SDL.h>
#endif
#include <filesystem>

namespace rb::gfx
{

vkres<VkImageView> create_image_view(
    device& dev,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect,
    VkImageViewType type,
    uint32_t base_layer,
    uint32_t layer_count,
    uint32_t base_level,
    uint32_t level_count
){
    VkImageView view;
    VkImageViewCreateInfo view_info = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        {},
        image,
        type,
        format,
        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
         VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        {aspect, base_level, level_count, base_layer, layer_count}
    };
    vkCreateImageView(dev.logical_device, &view_info, nullptr, &view);

    dev.gc.depend(image, view);
    RB_GC_LABEL(dev.gc, view, "VkImageView");
    return {dev, view};
}

vkres<VkDescriptorSetLayout> create_descriptor_set_layout(
    device& dev,
    argvec<VkDescriptorSetLayoutBinding> bindings,
    argvec<VkDescriptorBindingFlags> binding_flags,
    bool push_descriptor_set
){
    VkDescriptorSetLayoutBindingFlagsCreateInfo flag_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        nullptr,
        (uint32_t)binding_flags.size(), binding_flags.data()
    };
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        &flag_info, {}, (uint32_t)bindings.size(), bindings.data()
    };
    if(push_descriptor_set)
        descriptor_set_layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    VkDescriptorSetLayout layout;
    vkCreateDescriptorSetLayout(
        dev.logical_device,
        &descriptor_set_layout_info,
        nullptr,
        &layout
    );
    RB_GC_LABEL(dev.gc, layout, "VkDescriptorSetLayout");
    return {dev, layout};
}

VkSemaphore create_binary_semaphore(device& dev)
{
    VkSemaphoreCreateInfo sem_info = {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0
    };
    VkSemaphore sem;
    vkCreateSemaphore(dev.logical_device, &sem_info, nullptr, &sem);
    RB_GC_LABEL(dev.gc, sem, "VkSemaphore (binary)");
    return sem;
}

vkres<VkSemaphore> create_timeline_semaphore(device& dev, uint64_t start_value)
{
    VkSemaphoreTypeCreateInfo sem_type_info = {
      VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr,
      VK_SEMAPHORE_TYPE_TIMELINE, 0
    };
    VkSemaphoreCreateInfo sem_info = {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &sem_type_info, 0
    };
    VkSemaphore sem;
    vkCreateSemaphore(dev.logical_device, &sem_info, nullptr, &sem);
    RB_GC_LABEL(dev.gc, sem, "VkSemaphore (timeline)");
    return {dev, sem};
}

bool wait_timeline_semaphore(device& dev, VkSemaphore sem, uint64_t wait_value, uint64_t timeout)
{
    VkSemaphoreWaitInfo wait_info = {
        VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO, nullptr, 0, 1, &sem, &wait_value
    };
    return vkWaitSemaphores(dev.logical_device, &wait_info, timeout) == VK_SUCCESS;
}

vkres<VkBuffer> create_gpu_buffer(device& dev, size_t bytes, VkBufferUsageFlags usage, size_t min_alignment)
{
    if(bytes == 0)
        return vkres<VkBuffer>();

    VkBufferCreateInfo info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        nullptr,
        0,
        bytes,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr
    };
    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VkBuffer buffer;
    VmaAllocation alloc;
    if(min_alignment == 0)
    {
        vmaCreateBuffer(
            dev.allocator, &info,
            &alloc_info, &buffer,
            &alloc, nullptr
        );
    }
    else
    {
        vmaCreateBufferWithAlignment(
            dev.allocator, &info,
            &alloc_info, min_alignment,
            &buffer, &alloc, nullptr
        );
    }

    RB_GC_LABEL(dev.gc, buffer, " VkBuffer, alignment ", min_alignment, ", size ", bytes, " usage: ", usage);

    return vkres<VkBuffer>(dev, buffer, alloc);
}

vkres<VkBuffer> create_staging_buffer(
    device& dev,
    size_t bytes,
    const void* initial_data
){
    VkBufferCreateInfo info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        nullptr,
        0,
        bytes,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr
    };
    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VkBuffer buffer;
    VmaAllocation alloc;
    vmaCreateBuffer(
        dev.allocator, &info,
        &alloc_info, &buffer,
        &alloc, nullptr
    );

    if(initial_data)
    {
        void* mapped = nullptr;
        vmaMapMemory(dev.allocator, alloc, &mapped);
        memcpy(mapped, initial_data, bytes);
        vmaUnmapMemory(dev.allocator, alloc);
    }

    RB_GC_LABEL(dev.gc, buffer, " VkBuffer (staging) size ", bytes);

    return vkres<VkBuffer>(dev, buffer, alloc);
}

vkres<VkBuffer> create_readback_buffer(device& dev, size_t bytes)
{
    VkBufferCreateInfo info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        nullptr,
        0,
        bytes,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr
    };
    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

    VkBuffer buffer;
    VmaAllocation alloc;
    vmaCreateBuffer(
        dev.allocator, &info,
        &alloc_info, &buffer,
        &alloc, nullptr
    );

    RB_GC_LABEL(dev.gc, buffer, " VkBuffer (readback) size ", bytes);

    return vkres<VkBuffer>(dev, buffer, alloc);
}

vkres<VkImage> create_gpu_image(
    device& dev,
    event& result_event,
    uvec3 size,
    unsigned array_layers,
    VkFormat format,
    VkImageLayout layout,
    VkSampleCountFlagBits samples,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkImageViewType type,
    size_t bytes,
    void* data,
    bool mipmapped
){
    if(data)
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if(mipmapped)
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkImageCreateInfo info = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        type == VK_IMAGE_VIEW_TYPE_CUBE ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u,
        type == VK_IMAGE_VIEW_TYPE_3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
        format,
        {size.x, size.y, size.z},
        mipmapped ? calculate_mipmap_count(size) : 1,
        type == VK_IMAGE_VIEW_TYPE_CUBE ? 6u : array_layers,
        samples,
        tiling,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED
    };
    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    VkImage img;
    VmaAllocation alloc;

    vmaCreateImage(
        dev.allocator, (VkImageCreateInfo*)&info,
        &alloc_info, &img,
        &alloc, nullptr
    );

    vkres<VkCommandBuffer> cmd = begin_command_buffer(dev);

    image_barrier(
        cmd, img, format, VK_IMAGE_LAYOUT_UNDEFINED,
        !data ? layout : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    vkres<VkBuffer> buf;
    if(data)
    {
        buf = create_staging_buffer(dev, bytes, data);
        cmd.depend({*buf, img});

        VkBufferImageCopy copy = {
            0, 0, 0,
            {deduce_image_aspect_flags(format), 0, 0, 1},
            {0, 0, 0},
            {size.x, size.y, size.z}
        };

        size_t mip_levels = 1;

        uvec3 dim = size;
        size_t pixel_size = get_format_size(format);
        size_t array_pixel_size = array_layers * pixel_size;
        size_t mipped_bytes = dim.x * dim.y * dim.z * array_pixel_size;
        if(mipmapped)
        {
            while(dim != uvec3(1))
            {
                dim = max(dim/2u, uvec3(1));
                mip_levels++;
                mipped_bytes += dim.x * dim.y * dim.z * array_pixel_size;
            }
        }

        if(mipped_bytes > bytes)
            mip_levels = 1;

        size_t offset = 0;
        std::vector<VkBufferImageCopy> copies;
        dim = size;
        for(uint32_t mip = 0; mip < mip_levels; ++mip)
        {
            for(uint32_t layer = 0; layer < array_layers; ++layer)
            {
                copies.push_back({
                    offset, 0, 0,
                    {VK_IMAGE_ASPECT_COLOR_BIT, mip, layer, 1},
                    {0, 0, 0},
                    {dim.x, dim.y, dim.z}
                });

                offset += dim.x * dim.y * dim.z * pixel_size;
            }
            dim = max(dim/2u, uvec3(1));
        }

        vkCmdCopyBufferToImage(
            cmd, buf, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copies.size(), copies.data()
        );

        if(mipmapped && mip_levels == 1)
            generate_mipmaps(
                cmd, img, format, size, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout
            );
        else
        {
            image_barrier(
                cmd, img, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout
            );
        }
    }

    result_event = end_command_buffer(dev, cmd);

    RB_GC_LABEL(dev.gc, img, " VkImage");

    return vkres<VkImage>(dev, img, alloc);
}

void generate_mipmaps(
    VkCommandBuffer cmd,
    VkImage img,
    VkFormat format,
    uvec2 size,
    VkImageLayout before,
    VkImageLayout after
){
    unsigned mipmap_count = calculate_mipmap_count(size);
    if(before != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        image_barrier(
            cmd, img, format, before, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
    }

    for(unsigned i = 1; i < mipmap_count; ++i)
    {
        image_barrier(
            cmd, img, format,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            i-1, 1
        );
        uvec2 next_size = max(size/2u, uvec2(1));
        VkImageBlit blit = {
            {VK_IMAGE_ASPECT_COLOR_BIT, i-1, 0, 1},
            {{0,0,0}, {(int32_t)size.x, (int32_t)size.y, 1}},
            {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1},
            {{0,0,0}, {(int32_t)next_size.x, (int32_t)next_size.y, 1}}
        };
        vkCmdBlitImage(
            cmd,
            img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR
        );
        size = next_size;
        image_barrier(
            cmd, img, format, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, after, i-1, 1
        );
    }

    image_barrier(
        cmd, img, format,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        after,
        mipmap_count-1, 1
    );
}

event copy_buffer(
    device& dev,
    VkBuffer dst_buffer,
    VkBuffer src_buffer,
    size_t bytes
){
    vkres<VkCommandBuffer> buf = begin_command_buffer(dev);
    VkBufferCopy region = {0, 0, bytes};
    vkCmdCopyBuffer(buf, src_buffer, dst_buffer, 1, &region);
    buf.depend({dst_buffer, src_buffer});
    return end_command_buffer(dev, buf);
}

vkres<VkBuffer> upload_buffer(
    device& dev,
    event& result_event,
    size_t bytes,
    const void* data,
    VkBufferUsageFlags usage
){
    vkres<VkBuffer> staging = create_staging_buffer(dev, bytes, data);

    VkBufferCreateInfo info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        nullptr,
        0,
        bytes,
        usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr
    };
    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VkBuffer buffer;
    VmaAllocation alloc;
    vmaCreateBuffer(
        dev.allocator, &info,
        &alloc_info, &buffer,
        &alloc, nullptr
    );
    result_event = copy_buffer(dev, buffer, staging, bytes);
    RB_GC_LABEL(dev.gc, buffer, " VkBuffer (uploaded on creation)");
    return vkres<VkBuffer>(dev, buffer, alloc);
}

vkres<VkCommandBuffer> begin_command_buffer(device& dev)
{
    VkCommandPool pool = dev.get_graphics_pool();
    VkCommandBuffer buf = dev.allocate_command_buffer(pool);
    VkCommandBufferBeginInfo begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        nullptr
    };
    vkBeginCommandBuffer(buf, &begin_info);
    RB_GC_LABEL(dev.gc, buf, " unmanaged command buffer");
    return vkres<VkCommandBuffer>(dev, pool, buf);
}

event end_command_buffer(device& dev, VkCommandBuffer buf)
{
    vkEndCommandBuffer(buf);
    return dev.queue_graphics(dev.get_graphics_async_load_event(), buf, true);
}

size_t calculate_descriptor_pool_sizes(
    size_t binding_count,
    VkDescriptorPoolSize* pool_sizes,
    const VkDescriptorSetLayoutBinding* bindings,
    uint32_t multiplier
){
    size_t count = 0;
    for(size_t i = 0; i < binding_count; ++i)
    {
        VkDescriptorSetLayoutBinding b = bindings[i];
        bool found = false;
        for(size_t j = 0; j < count; ++j)
        {
            VkDescriptorPoolSize& size = pool_sizes[j];
            if(size.type == b.descriptorType)
            {
                found = true;
                size.descriptorCount += b.descriptorCount * multiplier;
            }
        }

        if(!found)
        {
            pool_sizes[count] = {b.descriptorType, b.descriptorCount * multiplier};
            count++;
        }
    }
    return count;
}

void image_barrier(
    VkCommandBuffer cmd,
    VkImage image,
    VkFormat format,
    VkImageLayout layout_before,
    VkImageLayout layout_after,
    uint32_t mip_level,
    uint32_t mip_count,
    uint32_t array_level,
    uint32_t array_count,
    VkAccessFlags2KHR happens_before,
    VkAccessFlags2KHR happens_after,
    VkPipelineStageFlags2KHR stage_before,
    VkPipelineStageFlags2KHR stage_after
){
    VkImageMemoryBarrier2KHR image_barrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
        nullptr,
        stage_before,
        happens_before,
        stage_after,
        happens_after,
        layout_before,
        layout_after,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        image,
        {
            deduce_image_aspect_flags(format),
            mip_level, mip_count,
            array_level, array_count
        }
    };
    VkDependencyInfoKHR dependency_info = {
        VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        nullptr,
        0,
        0, nullptr,
        0, nullptr,
        1, &image_barrier
    };
    vkCmdPipelineBarrier2KHR(cmd, &dependency_info);
}

void buffer_barrier(
    VkCommandBuffer cmd,
    VkBuffer buf,
    VkAccessFlags2KHR happens_before,
    VkAccessFlags2KHR happens_after,
    VkPipelineStageFlags2KHR stage_before,
    VkPipelineStageFlags2KHR stage_after
){
    VkBufferMemoryBarrier2KHR buffer_barrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
        nullptr,
        stage_before,
        happens_before,
        stage_after,
        happens_after,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        buf,
        0,
        VK_WHOLE_SIZE
    };
    VkDependencyInfoKHR dependency_info = {
        VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        nullptr,
        0,
        0, nullptr,
        1, &buffer_barrier,
        0, nullptr
    };
    vkCmdPipelineBarrier2KHR(cmd, &dependency_info);
}

void full_barrier(VkCommandBuffer cmd)
{
    VkMemoryBarrier2KHR barrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
        nullptr,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
        VK_ACCESS_2_MEMORY_WRITE_BIT_KHR | VK_ACCESS_2_MEMORY_READ_BIT_KHR,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
        VK_ACCESS_2_MEMORY_WRITE_BIT_KHR | VK_ACCESS_2_MEMORY_READ_BIT_KHR
    };
    VkDependencyInfoKHR dependency_info = {
        VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        nullptr,
        0,
        1, &barrier,
        0, nullptr,
        0, nullptr
    };
    vkCmdPipelineBarrier2KHR(cmd, &dependency_info);
}

void interlace(void* dst, const void* src, const void* fill, size_t src_stride, size_t dst_stride, size_t entries)
{
    for(size_t i = 0; i < entries; ++i)
    {
        memcpy((uint8_t*)dst+dst_stride*i, (uint8_t*)src+src_stride*i, src_stride);
        memcpy((uint8_t*)dst+dst_stride*i+src_stride, fill, dst_stride-src_stride);
    }
}

VkImageAspectFlags deduce_image_aspect_flags(VkFormat format)
{
    switch(format)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

VkImageUsageFlags deduce_image_usage_flags(
    VkFormat format,
    bool read,
    bool write
){
    VkImageUsageFlags flags = 0;
    if(write)
    {
        switch(format)
        {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            break;
        default:
            flags |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            break;
        }
        flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if(read)
    {
        flags |= VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    return flags;
}

// Adapted from: https://gist.github.com/rygorous/2156668 (public domain)
float half_to_float(uint16_t half)
{
    union FP32
    {
        uint32_t u;
        float f;
        struct
        {
            uint32_t Mantissa : 23;
            uint32_t Exponent : 8;
            uint32_t Sign : 1;
        };
    };

    static const FP32 magic = { 113 << 23 };
    static const uint shifted_exp = 0x7c00 << 13; // exponent mask after shift
    FP32 o;

    o.u = (half & 0x7fff) << 13;     // exponent/mantissa bits
    uint exp = shifted_exp & o.u;   // just the exponent
    o.u += (127 - 15) << 23;        // exponent adjust

    // handle exponent special cases
    if (exp == shifted_exp) // Inf/NaN?
        o.u += (128 - 16) << 23;    // extra exp adjust
    else if (exp == 0) // Zero/Denormal?
    {
        o.u += 1 << 23;             // extra exp adjust
        o.f -= magic.f;             // renormalize
    }

    o.u |= (half & 0x8000) << 16;    // sign bit
    return o.f;
}

size_t get_format_channels(VkFormat format)
{
    return vk::componentCount(vk::Format(format));
}

size_t get_format_channel_size(VkFormat format, unsigned channel_index)
{
    if(channel_index >= get_format_channels(format))
        return 0;
    return vk::componentBits(vk::Format(format), channel_index)/8;
}

size_t get_format_size(VkFormat format)
{
    size_t size = 0;
    for(size_t i = 0; i < get_format_channels(format); ++i)
        size += vk::componentBits(vk::Format(format), i);
    return size/8;
}

VkFormat get_format_channel_type(VkFormat format)
{
    switch(format)
    {
    default:
        RB_PANIC("Unhandled format ", format);
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        return VK_FORMAT_R8_UNORM;
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
        return VK_FORMAT_R8_SNORM;
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
        return VK_FORMAT_R8_USCALED;
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
        return VK_FORMAT_R8_SSCALED;
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_S8_UINT:
        return VK_FORMAT_R8_UINT;
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
        return VK_FORMAT_R8_SINT;
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        return VK_FORMAT_R8_SRGB;
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_D16_UNORM:
        return VK_FORMAT_R16_UNORM;
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
        return VK_FORMAT_R16_SNORM;
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16A16_USCALED:
        return VK_FORMAT_R16_USCALED;
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
        return VK_FORMAT_R16_SSCALED;
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16A16_UINT:
        return VK_FORMAT_R16_UINT;
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16A16_SINT:
        return VK_FORMAT_R16_SINT;
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return VK_FORMAT_R16_SFLOAT;
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32A32_UINT:
        return VK_FORMAT_R32_UINT;
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32A32_SINT:
        return VK_FORMAT_R32_SINT;
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT:
        return VK_FORMAT_R32_SFLOAT;
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64G64_UINT:
    case VK_FORMAT_R64G64B64_UINT:
    case VK_FORMAT_R64G64B64A64_UINT:
        return VK_FORMAT_R64_UINT;
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64G64B64_SINT:
    case VK_FORMAT_R64G64B64A64_SINT:
        return VK_FORMAT_R64_SINT;
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_R64G64B64A64_SFLOAT:
        return VK_FORMAT_R64_SFLOAT;
    }
}

bool format_is_simple(VkFormat format)
{
    uint8_t bits = vk::componentBits(vk::Format(format), 0);
    if(bits != 8 && bits != 16 && bits != 32 && bits != 64)
        return false;
    for(size_t i = 1; i < get_format_channels(format); ++i)
    {
        if(vk::componentBits(vk::Format(format), i) != bits)
            return false;
    }
    return !vk::componentsAreCompressed(vk::Format(format));
}

bool format_is_depth(VkFormat format)
{
    switch(format)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

bool format_is_depth_stencil(VkFormat format)
{
    switch(format)
    {
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

template<typename T>
void copy_channel_data(T& t, void* data, VkFormat format)
{
    switch(format)
    {
    default:
        RB_PANIC("Unhandled format ", format);
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SRGB:
        t = *reinterpret_cast<uint8_t*>(data)/255.0f;
        break;
    case VK_FORMAT_R8_SNORM:
        t = *reinterpret_cast<int8_t*>(data)/127.0f;
        break;
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_UINT:
        t = *reinterpret_cast<uint8_t*>(data);
        break;
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_SINT:
        t = *reinterpret_cast<int8_t*>(data);
        break;
    case VK_FORMAT_R16_UNORM:
        t = *reinterpret_cast<uint16_t*>(data)/65535.0f;
        break;
    case VK_FORMAT_R16_SNORM:
        t = *reinterpret_cast<int16_t*>(data)/32767.0f;
        break;
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16_UINT:
        t = *reinterpret_cast<uint16_t*>(data);
        break;
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_SINT:
        t = *reinterpret_cast<int16_t*>(data);
        break;
    case VK_FORMAT_R16_SFLOAT:
        t = half_to_float(*reinterpret_cast<uint16_t*>(data));
        break;
    case VK_FORMAT_R32_UINT:
        t = *reinterpret_cast<uint32_t*>(data);
        break;
    case VK_FORMAT_R32_SINT:
        t = *reinterpret_cast<int32_t*>(data);
        break;
    case VK_FORMAT_R32_SFLOAT:
        t = *reinterpret_cast<float*>(data);
        break;
    case VK_FORMAT_R64_UINT:
        t = *reinterpret_cast<uint64_t*>(data);
        break;
    case VK_FORMAT_R64_SINT:
        t = *reinterpret_cast<int64_t*>(data);
        break;
    case VK_FORMAT_R64_SFLOAT:
        t = *reinterpret_cast<double*>(data);
        break;
    }
}

template<typename T>
std::vector<T> read_formatted_data(size_t size, const void* data, VkFormat format)
{
    if(!format_is_simple(format))
        RB_PANIC("This function only works with simple formats");

    size_t channels = get_format_channels(format);
    size_t desired_channels = 1;
    if constexpr(
        std::is_same_v<T, pvec2> ||
        std::is_same_v<T, pivec2> ||
        std::is_same_v<T, puvec2>
    ){
        desired_channels = 2;
    } else if constexpr(
        std::is_same_v<T, pvec3> ||
        std::is_same_v<T, pivec3> ||
        std::is_same_v<T, puvec3>
    ){
        desired_channels = 3;
    } else if constexpr(
        std::is_same_v<T, pvec4> ||
        std::is_same_v<T, pivec4> ||
        std::is_same_v<T, puvec4>
    ){
        desired_channels = 4;
    }
    desired_channels = std::min(channels, desired_channels);

    VkFormat channel_format = get_format_channel_type(format);
    size_t channel_size = get_format_channel_size(format);
    size_t pixel_size = channels * channel_size;
    size_t pixel_count = size/pixel_size;
    std::vector<T> res(pixel_count);
    for(size_t i = 0; i < pixel_count; ++i)
    {
        T& pixel = res[i];
        uint8_t* pixel_data = ((uint8_t*)data) + i * pixel_size;
        for(size_t j = 0; j < desired_channels; ++j)
        {
            uint8_t* channel_data = pixel_data + j * channel_size;
            if constexpr(std::is_arithmetic_v<T>)
            {
                copy_channel_data(pixel, channel_data, channel_format);
            }
            else
            {
                copy_channel_data(pixel[j], channel_data, channel_format);
            }
        }
    }
    return res;
}

template std::vector<float> read_formatted_data(size_t, const void*, VkFormat);
template std::vector<pvec2> read_formatted_data(size_t, const void*, VkFormat);
template std::vector<pvec3> read_formatted_data(size_t, const void*, VkFormat);
template std::vector<pvec4> read_formatted_data(size_t, const void*, VkFormat);
template std::vector<int32_t> read_formatted_data(size_t, const void*, VkFormat);
template std::vector<pivec2> read_formatted_data(size_t, const void*, VkFormat);
template std::vector<pivec3> read_formatted_data(size_t, const void*, VkFormat);
template std::vector<pivec4> read_formatted_data(size_t, const void*, VkFormat);
template std::vector<uint32_t> read_formatted_data(size_t, const void*, VkFormat);
template std::vector<puvec2> read_formatted_data(size_t, const void*, VkFormat);
template std::vector<puvec3> read_formatted_data(size_t, const void*, VkFormat);
template std::vector<puvec4> read_formatted_data(size_t, const void*, VkFormat);

#ifdef RAYBASE_HAS_SDL2
SDL_Surface* load_image(const char* path)
{
    int w, h, dummy;
    uint8_t* data = stbi_load(path, &w, &h, &dummy, STBI_rgb_alpha);
    if(!data) RB_PANIC("Failed to read icon from ", path);

    return SDL_CreateRGBSurfaceWithFormatFrom(
        (void*)data, w, h,
        32, sizeof(uint32_t) * w,
        SDL_PIXELFORMAT_RGBA32
    );
}
#endif

void write_ldr_image(std::string_view path, const void* data, uvec2 size, uint32_t channels)
{
    std::filesystem::path p(path);
    if(p.extension() == ".png")
        stbi_write_png(p.string().c_str(), size.x, size.y, channels, data, size.x*channels);
    else if(p.extension() == ".bmp")
        stbi_write_bmp(p.string().c_str(), size.x, size.y, channels, data);
    else if(p.extension() == ".tga")
        stbi_write_tga(p.string().c_str(), size.x, size.y, channels, data);
    else if(p.extension() == ".jpg")
        stbi_write_jpg(p.string().c_str(), size.x, size.y, channels, data, 95);
    else RB_PANIC("Can't save LDR image, unsupported image file extension: ", path);
}

void write_image(std::string_view path, const void* data, VkFormat format, uvec2 size)
{
    std::filesystem::path p(path);
    bool is_hdr = p.extension() == ".hdr";

    switch(format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        write_ldr_image(path, data, size, 4);
        return;
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SRGB:
        write_ldr_image(path, data, size, 3);
        return;
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SRGB:
        write_ldr_image(path, data, size, 2);
        return;
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SRGB:
        write_ldr_image(path, data, size, 1);
        return;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        if(is_hdr)
        {
            stbi_write_hdr(p.string().c_str(), size.x, size.y, 4, (const float*)data);
            return;
        }
        break;
    case VK_FORMAT_R32G32B32_SFLOAT:
        if(is_hdr)
        {
            stbi_write_hdr(p.string().c_str(), size.x, size.y, 3, (const float*)data);
            return;
        }
        break;
    case VK_FORMAT_R32G32_SFLOAT:
        if(is_hdr)
        {
            stbi_write_hdr(p.string().c_str(), size.x, size.y, 2, (const float*)data);
            return;
        }
        break;
    case VK_FORMAT_R32_SFLOAT:
        if(is_hdr)
        {
            stbi_write_hdr(p.string().c_str(), size.x, size.y, 1, (const float*)data);
            return;
        }
        break;
    default:
        break;
    }
    // If we're here, we're not on the happy path anymore. Now we have to do
    // some conversions.

    uint32_t channel_count = get_format_channels(format);
    std::vector<pvec4> values = read_formatted_data<pvec4>(
        size.x * size.y * get_format_size(format),
        data,
        format
    );

    if(is_hdr)
    {
        std::vector<float> colors(values.size() * channel_count);
        for(size_t i = 0; i < size.x * size.y; ++i)
        {
            for(uint32_t c = 0; c < channel_count; ++c)
                colors[i*channel_count+c] = values[i][c];
        }
        stbi_write_hdr(p.string().c_str(), size.x, size.y, channel_count, (const float*)data);
    }
    else
    {
        std::vector<uint8_t> colors(values.size() * channel_count);
        for(size_t i = 0; i < size.x * size.y; ++i)
        {
            for(uint32_t c = 0; c < channel_count; ++c)
                colors[i*channel_count+c] = round(clamp(values[i][c] * 255.0f, 0.0f, 255.0f));
        }
        write_ldr_image(path, colors.data(), size, channel_count);
    }
}

event async_save_image(
    device& dev, std::string path, VkImage image, VkImageLayout layout,
    VkFormat format, uvec2 size, argvec<event> wait_for, bool flip
){
    size_t bytes = size.x * size.y * get_format_size(format);
    vkres<VkBuffer> read_buf = create_readback_buffer(dev, bytes);

    vkres<VkCommandBuffer> cmd = begin_command_buffer(dev);

    image_barrier(cmd, image, format, layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy copy = {
        0, 0, 0,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        {0, 0, 0},
        {size.x, size.y, 1}
    };
    vkCmdCopyImageToBuffer(
        cmd,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        *read_buf, 1, &copy
    );
    image_barrier(cmd, image, format, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout);

    vkEndCommandBuffer(cmd);
    event res = dev.queue_graphics(wait_for, *cmd, true);

    auto copyable_read_buf = std::make_shared<vkres<VkBuffer>>(std::move(read_buf));
    auto write_callback = [path, dev=&dev, read_buf = copyable_read_buf, format, size, flip]() {
        const VmaAllocator& allocator = dev->allocator;
        void* data;
        vmaMapMemory(allocator, read_buf->get_allocation(), &data);
        stbi_flip_vertically_on_write(flip);
        write_image(path, data, format, size);
        vmaUnmapMemory(allocator, read_buf->get_allocation());
        RB_LOG("Saved image ", path);
    };

    res.finish_callback(dev,
        [ctx = dev.ctx, write_callback](){
            ctx->get_thread_pool().add_task(write_callback);
        }
    );

    return res;
}

}
