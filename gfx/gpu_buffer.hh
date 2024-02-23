#ifndef RAYBASE_GFX_GPU_BUFFER_HH
#define RAYBASE_GFX_GPU_BUFFER_HH

#include "context.hh"
#include "vkres.hh"
#include <vector>
#include <type_traits>

namespace rb::gfx
{

// This class is specifically for easy-to-update GPU buffers; it abstracts
// staging buffers and in-flight frames. If your buffer doesn't update
// dynamically, this class isn't of much use and is likely just bloat.
class gpu_buffer
{
public:
    gpu_buffer(
        device& dev,
        size_t bytes = 0,
        VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        size_t alignment = 0
    );

    // Returns true if the resize truly occurred
    bool resize(size_t size);
    size_t get_size() const;

    operator VkBuffer() const;
    VkDeviceAddress get_device_address() const;
    VkBuffer get_staging_buffer(uint32_t frame_index) const;

    device& get_device() const;

    void update_ptr(uint32_t frame_index, const void* data, size_t bytes);
    template<typename T>
    void update(uint32_t frame_index, const T& t);
    template<typename T, typename F>
    void update(uint32_t frame_index, F&& f);
    void upload_individual(VkCommandBuffer cmd, uint32_t frame_index);

protected:
    device* dev;
    size_t bytes;
    size_t alignment;
    VkBufferUsageFlags usage;
    vkres<VkBuffer> buffer;
    std::vector<vkres<VkBuffer>> staging_buffers;
};

template<typename T, typename F>
void gpu_buffer::update(uint32_t frame_index, F&& f)
{
    if(staging_buffers.size() == 0) return;

    T* dst = nullptr;
    vkres<VkBuffer>& staging = staging_buffers[frame_index];
    const VmaAllocator& allocator = dev->allocator;
    vmaMapMemory(allocator, staging.get_allocation(), (void**)&dst);
    f(dst);
    vmaUnmapMemory(allocator, staging.get_allocation());
}

template<typename T>
void gpu_buffer::update(uint32_t frame_index, const T& t)
{
    if constexpr(std::is_pointer_v<T>)
    {
        update_ptr(frame_index, t, bytes);
    }
    else
    {
        update_ptr(frame_index, &t, sizeof(T));
    }
}

}

#endif
