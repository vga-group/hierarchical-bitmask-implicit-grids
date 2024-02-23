#include "vkres.hh"
#include "device.hh"
#include <type_traits>

namespace rb::gfx
{

template<typename T>
vkres<T>::vkres()
: value(VK_NULL_HANDLE), dev(nullptr)
{
}

template<typename T>
vkres<T>::vkres(device& dev, T t)
: value(t), dev(&dev)
{
}

template<typename T>
vkres<T>::vkres(vkres<T>&& other) noexcept
: value(other.value), dev(other.dev)
{
    other.value = VK_NULL_HANDLE;
}

template<typename T>
vkres<T>::~vkres()
{
    reset();
}

template<typename T>
void vkres<T>::reset(T other)
{
    if(dev && value != VK_NULL_HANDLE && value != other)
    {
        VkDevice logical_device = dev->logical_device;
        if constexpr(std::is_same_v<T, VkSemaphore>)
        {
            dev->gc.remove(value);
        }
        else
        {
            dev->gc.remove(value, [value=value, logical_device=logical_device](){
#define destroy_type(type) \
                else if constexpr(std::is_same_v<T, Vk##type>) { \
                    /*printf("Destroying a " #type "\n");*/ \
                    vkDestroy##type(logical_device, value, nullptr);\
                }
                if constexpr(false) {}
                destroy_type(AccelerationStructureKHR)
                destroy_type(Buffer)
                destroy_type(BufferView)
                destroy_type(CommandPool)
                destroy_type(DescriptorPool)
                destroy_type(DescriptorSetLayout)
                destroy_type(Framebuffer)
                destroy_type(Image)
                destroy_type(ImageView)
                destroy_type(Pipeline)
                destroy_type(PipelineCache)
                destroy_type(PipelineLayout)
                destroy_type(QueryPool)
                destroy_type(RenderPass)
                destroy_type(Sampler)
                destroy_type(Semaphore)
                destroy_type(ShaderModule)
                destroy_type(DescriptorUpdateTemplate)
                else static_assert("Unknown Vulkan resource type!");
            });
        }
    }
    value = other;
}

template<typename T>
void vkres<T>::operator=(vkres<T>&& other) noexcept
{
    reset(other.value);
    dev = other.dev;
    other.value = VK_NULL_HANDLE;
}

template<typename T>
T vkres<T>::leak()
{
    dev = nullptr;
    return value;
}

template<typename T>
const T& vkres<T>::operator*() const
{
    return value;
}

template<typename T>
vkres<T>::operator T() const
{
    return value;
}

vkres<VkCommandBuffer>::vkres()
: value(VK_NULL_HANDLE), pool(VK_NULL_HANDLE), dev(nullptr)
{
}

vkres<VkCommandBuffer>::vkres(device& dev, VkCommandPool pool, VkCommandBuffer buf)
: value(buf), pool(pool), dev(&dev)
{
}

vkres<VkCommandBuffer>::vkres(vkres<VkCommandBuffer>&& other) noexcept
: value(other.value), pool(other.pool), dev(other.dev)
{
    other.value = VK_NULL_HANDLE;
}

vkres<VkCommandBuffer>::~vkres()
{
    reset();
}

void vkres<VkCommandBuffer>::depend(argvec<void*> resource)
{
    if(dev && value != VK_NULL_HANDLE)
    {
        for(void* v: resource)
            dev->gc.depend(v, value);
    }
}

void vkres<VkCommandBuffer>::reset(VkCommandBuffer other)
{
    if(pool != VK_NULL_HANDLE && dev && value != VK_NULL_HANDLE && value != other)
    {
        VkDevice logical_device = dev->logical_device;
        dev->gc.remove(value, [value=value, pool=pool, dev=dev](){
            // This is a difficult bastard. We can't use a pool simultaneously
            // from multiple threads, yet we'll need to be able to release
            // command buffers from any pool on the garbage collector thread.
            dev->add_command_buffer_garbage(pool, value);
        });
    }
    value = other;
}

void vkres<VkCommandBuffer>::operator=(vkres<VkCommandBuffer>&& other) noexcept
{
    reset(other.value);
    dev = other.dev;
    pool = other.pool;
    other.value = VK_NULL_HANDLE;
}

const VkCommandBuffer& vkres<VkCommandBuffer>::operator*() const
{
    return value;
}

vkres<VkCommandBuffer>::operator VkCommandBuffer() const
{
    return value;
}

VkCommandPool vkres<VkCommandBuffer>::get_pool() const
{
    return pool;
}

vkres<VkDescriptorSet>::vkres()
: value(VK_NULL_HANDLE), pool(VK_NULL_HANDLE), dev(nullptr)
{
}

vkres<VkDescriptorSet>::vkres(device& dev, VkDescriptorPool pool, VkDescriptorSet set)
: value(set), pool(pool), dev(&dev)
{
}

vkres<VkDescriptorSet>::vkres(vkres<VkDescriptorSet>&& other) noexcept
: value(other.value), pool(other.pool), dev(other.dev)
{
    other.value = VK_NULL_HANDLE;
}

vkres<VkDescriptorSet>::~vkres()
{
    reset();
}

void vkres<VkDescriptorSet>::reset(VkDescriptorSet other)
{
    if(pool != VK_NULL_HANDLE && dev && value != VK_NULL_HANDLE && value != other)
    {
        VkDevice logical_device = dev->logical_device;
        dev->gc.remove(value, [value=value, pool=pool, logical_device=logical_device](){
            //printf("Destroying a DescriptorSet\n");
            vkFreeDescriptorSets(logical_device, pool, 1, &value);
        });
    }
    value = other;
}

void vkres<VkDescriptorSet>::operator=(vkres<VkDescriptorSet>&& other) noexcept
{
    reset(other.value);
    dev = other.dev;
    pool = other.pool;
    other.value = VK_NULL_HANDLE;
}

const VkDescriptorSet& vkres<VkDescriptorSet>::operator*() const
{
    return value;
}

vkres<VkDescriptorSet>::operator VkDescriptorSet() const
{
    return value;
}

VkDescriptorPool vkres<VkDescriptorSet>::get_pool() const
{
    return pool;
}

vkres<VkBuffer>::vkres()
: buffer(VK_NULL_HANDLE), allocation(VK_NULL_HANDLE), dev(nullptr)
{
}

vkres<VkBuffer>::vkres(device& dev, VkBuffer buf, VmaAllocation alloc)
: buffer(buf), allocation(alloc), dev(&dev)
{
}

vkres<VkBuffer>::vkres(vkres<VkBuffer>&& other) noexcept
: dev(VK_NULL_HANDLE), buffer(VK_NULL_HANDLE)
{
    operator=(std::move(other));
}

vkres<VkBuffer>::~vkres()
{
    reset();
}

void vkres<VkBuffer>::reset(VkBuffer buf, VmaAllocation alloc)
{
    if(dev && buffer)
    {
        dev->gc.remove(buffer, [
            buffer=this->buffer,
            allocation=this->allocation,
            logical_device=dev->logical_device,
            allocator=dev->allocator
        ](){
            //printf("Destroying a Buffer %p\n", buffer);
            vkDestroyBuffer(logical_device, buffer, nullptr);
            if(allocation != VK_NULL_HANDLE)
                vmaFreeMemory(allocator, allocation);
        });
    }
    buffer = buf;
    allocation = alloc;
}

void vkres<VkBuffer>::operator=(vkres<VkBuffer>&& other) noexcept
{
    reset();
    this->buffer = other.buffer;
    this->allocation = other.allocation;
    this->dev = other.dev;
    other.buffer = VK_NULL_HANDLE;
    other.allocation = VK_NULL_HANDLE;
}

const VkBuffer& vkres<VkBuffer>::operator*() const
{
    return buffer;
}

vkres<VkBuffer>::operator VkBuffer() const
{
    return buffer;
}

VmaAllocation vkres<VkBuffer>::get_allocation() const
{
    return allocation;
}

device* vkres<VkBuffer>::get_device() const
{
    return dev;
}

vkres<VkImage>::vkres()
:   image(VK_NULL_HANDLE), allocation(VK_NULL_HANDLE), memory(VK_NULL_HANDLE),
    dev(nullptr)
{
}

vkres<VkImage>::vkres(device& dev, VkImage img, VmaAllocation alloc)
:   image(img), allocation(alloc), memory(VK_NULL_HANDLE), dev(&dev)
{
}

vkres<VkImage>::vkres(device& dev, VkImage img, VkDeviceMemory memory)
:   image(img), allocation(VK_NULL_HANDLE), memory(memory), dev(&dev)
{
}

vkres<VkImage>::vkres(vkres<VkImage>&& other) noexcept
: dev(VK_NULL_HANDLE)
{
    operator=(std::move(other));
}

vkres<VkImage>::~vkres()
{
    reset();
}

void vkres<VkImage>::reset(VkImage img, VmaAllocation alloc)
{
    if(dev && image)
    {
        dev->gc.remove(image, [
            image=this->image,
            allocation=this->allocation,
            memory=this->memory,
            logical_device=dev->logical_device,
            allocator=dev->allocator
        ](){
            //printf("Destroying a Image\n");
            vkDestroyImage(logical_device, image, nullptr);
            if(allocation != VK_NULL_HANDLE)
                vmaFreeMemory(allocator, allocation);
            if(memory != VK_NULL_HANDLE)
                vkFreeMemory(logical_device, memory, nullptr);
        });
    }
    image = img;
    allocation = alloc;
}

void vkres<VkImage>::operator=(vkres<VkImage>&& other) noexcept
{
    reset();
    this->image = other.image;
    this->allocation = other.allocation;
    this->memory = other.memory;
    this->dev = other.dev;
    other.image = VK_NULL_HANDLE;
    other.allocation = VK_NULL_HANDLE;
    other.memory = VK_NULL_HANDLE;
}

const VkImage& vkres<VkImage>::operator*() const
{
    return image;
}

vkres<VkImage>::operator VkImage() const
{
    return image;
}

template class vkres<VkAccelerationStructureKHR>;
template class vkres<VkBufferView>;
template class vkres<VkCommandPool>;
template class vkres<VkDescriptorPool>;
template class vkres<VkDescriptorSetLayout>;
template class vkres<VkFramebuffer>;
template class vkres<VkImageView>;
template class vkres<VkPipeline>;
template class vkres<VkPipelineCache>;
template class vkres<VkPipelineLayout>;
template class vkres<VkQueryPool>;
template class vkres<VkRenderPass>;
template class vkres<VkSampler>;
template class vkres<VkSemaphore>;
template class vkres<VkShaderModule>;

}
