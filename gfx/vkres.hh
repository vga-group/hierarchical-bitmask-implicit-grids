#ifndef RAYBASE_GFX_VKRES_HH
#define RAYBASE_GFX_VKRES_HH

#include "volk.h"
#include "vk_mem_alloc.h"
#include "core/argvec.hh"

namespace rb::gfx
{

class device;

// Automatic safely deleted Vulkan resources
template<typename T>
class vkres
{
public:
    vkres();
    vkres(device& dev, T t = VK_NULL_HANDLE);
    vkres(const vkres<T>& other) = delete;
    vkres(vkres<T>&& other) noexcept;
    ~vkres();

    void reset(T other = VK_NULL_HANDLE);
    void operator=(vkres<T>&& other) noexcept;
    T leak();

    const T& operator*() const;
    operator T() const;

private:
    T value;
    device* dev;
};

template<>
class vkres<VkCommandBuffer>
{
public:
    vkres();
    vkres(device& dev, VkCommandPool pool, VkCommandBuffer buf = VK_NULL_HANDLE);
    vkres(const vkres<VkCommandBuffer>& other) = delete;
    vkres(vkres<VkCommandBuffer>&& other) noexcept;
    ~vkres();

    void depend(argvec<void*> resource);
    void reset(VkCommandBuffer other = VK_NULL_HANDLE);
    void operator=(vkres<VkCommandBuffer>&& other) noexcept;

    const VkCommandBuffer& operator*() const;
    operator VkCommandBuffer() const;

    VkCommandPool get_pool() const;

private:
    VkCommandBuffer value;
    VkCommandPool pool;
    device* dev;
};

template<>
class vkres<VkDescriptorSet>
{
public:
    vkres();
    vkres(device& dev, VkDescriptorPool pool, VkDescriptorSet set = VK_NULL_HANDLE);
    vkres(const vkres<VkDescriptorSet>& other) = delete;
    vkres(vkres<VkDescriptorSet>&& other) noexcept;
    ~vkres();

    void reset(VkDescriptorSet other = VK_NULL_HANDLE);
    void operator=(vkres<VkDescriptorSet>&& other) noexcept;

    const VkDescriptorSet& operator*() const;
    operator VkDescriptorSet() const;

    VkDescriptorPool get_pool() const;

private:
    VkDescriptorSet value;
    VkDescriptorPool pool;
    device* dev;
};

template<>
class vkres<VkBuffer>
{
public:
    vkres();
    vkres(device& dev, VkBuffer buf = VK_NULL_HANDLE, VmaAllocation alloc = VK_NULL_HANDLE);
    vkres(const vkres<VkBuffer>& other) = delete;
    vkres(vkres<VkBuffer>&& other) noexcept;
    ~vkres();

    void reset(VkBuffer buf = VK_NULL_HANDLE, VmaAllocation alloc = VK_NULL_HANDLE);
    void operator=(vkres<VkBuffer>&& other) noexcept;

    const VkBuffer& operator*() const;
    operator VkBuffer() const;

    VmaAllocation get_allocation() const;
    device* get_device() const;

private:
    VkBuffer buffer;
    VmaAllocation allocation;
    device* dev;
};

template<>
class vkres<VkImage>
{
public:
    vkres();
    vkres(device& dev, VkImage img = VK_NULL_HANDLE, VmaAllocation alloc = VK_NULL_HANDLE);
    vkres(device& dev, VkImage img, VkDeviceMemory memory);
    vkres(const vkres<VkImage>& other) = delete;
    vkres(vkres<VkImage>&& other) noexcept;
    ~vkres();

    void reset(VkImage img = VK_NULL_HANDLE, VmaAllocation alloc = VK_NULL_HANDLE);
    void operator=(vkres<VkImage>&& other) noexcept;

    const VkImage& operator*() const;
    operator VkImage() const;

private:
    VkImage image;
    VmaAllocation allocation;
    VkDeviceMemory memory;
    device* dev;
};

}

#endif
