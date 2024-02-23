#include "gpu_buffer.hh"
#include "vulkan_helpers.hh"

namespace rb::gfx
{

gpu_buffer::gpu_buffer(
    device& dev,
    size_t bytes,
    VkBufferUsageFlags usage,
    size_t alignment
): dev(&dev), bytes(0), alignment(alignment), usage(usage)
{
    resize(bytes);
}

bool gpu_buffer::resize(size_t size)
{
    if(this->bytes >= size) return false;

    this->bytes = size;
    staging_buffers.clear();

    buffer = create_gpu_buffer(*dev, bytes, usage|VK_BUFFER_USAGE_TRANSFER_DST_BIT, alignment);
    for(size_t i = 0; i < dev->get_in_flight_count(); ++i)
    {
        staging_buffers.emplace_back(create_staging_buffer(*dev, bytes));
        dev->gc.depend(*staging_buffers.back(), *buffer);
    }
    return true;
}

size_t gpu_buffer::get_size() const
{
    return bytes;
}

gpu_buffer::operator VkBuffer() const
{
    return *buffer;
}

VkDeviceAddress gpu_buffer::get_device_address() const
{
    VkBufferDeviceAddressInfo info = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, *buffer
    };
    VkDeviceAddress addr = vkGetBufferDeviceAddress(dev->logical_device, &info);
    return addr;
}

VkBuffer gpu_buffer::get_staging_buffer(uint32_t frame_index) const
{
    return staging_buffers[frame_index];
}

device& gpu_buffer::get_device() const
{
    return *dev;
}

void gpu_buffer::update_ptr(uint32_t frame_index, const void* data, size_t bytes)
{
    if(staging_buffers.size() == 0) return;

    if(bytes > this->bytes)
        bytes = this->bytes;

    void* dst = nullptr;
    vkres<VkBuffer>& staging = staging_buffers[frame_index];
    const VmaAllocator& allocator = dev->allocator;
    vmaMapMemory(allocator, staging.get_allocation(), &dst);
    memcpy(dst, data, bytes);
    vmaUnmapMemory(allocator, staging.get_allocation());
}

void gpu_buffer::upload_individual(VkCommandBuffer cmd, uint32_t frame_index)
{
    if(buffer)
    {
        VkBuffer source = staging_buffers[frame_index];
        VkBufferCopy copy = {0, 0, bytes};
        vkCmdCopyBuffer(cmd, source, *buffer, 1, &copy);
        buffer_barrier(cmd, *buffer);
        dev->gc.depend(*buffer, cmd);
    }
}

}
