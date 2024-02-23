#ifndef RAYBASE_GFX_VULKAN_HELPERS_HH
#define RAYBASE_GFX_VULKAN_HELPERS_HH

#include "device.hh"
#include "vkres.hh"
#include "event.hh"
#include "core/math.hh"
#include "core/argvec.hh"
#ifdef RAYBASE_HAS_SDL2
#include <SDL.h>
#endif

namespace rb::gfx
{

vkres<VkImageView> create_image_view(
    device& dev,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
    VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D,
    uint32_t base_layer = 0,
    uint32_t layer_count = VK_REMAINING_ARRAY_LAYERS,
    uint32_t base_level = 0,
    uint32_t level_count = VK_REMAINING_MIP_LEVELS
);

vkres<VkDescriptorSetLayout> create_descriptor_set_layout(
    device& dev,
    argvec<VkDescriptorSetLayoutBinding> bindings,
    argvec<VkDescriptorBindingFlags> binding_flags,
    bool push_descriptor_set
);

VkSemaphore create_binary_semaphore(device& dev);
vkres<VkSemaphore> create_timeline_semaphore(device& dev, uint64_t start_value = 0);
bool wait_timeline_semaphore(device& dev, VkSemaphore sem, uint64_t wait_value, uint64_t timeout = UINT64_MAX);
vkres<VkBuffer> create_gpu_buffer(device& dev, size_t bytes, VkBufferUsageFlags usage, size_t min_alignment = 0);
vkres<VkBuffer> create_staging_buffer(device& dev, size_t bytes, const void* initial_data = nullptr);
vkres<VkBuffer> create_readback_buffer(device& dev, size_t bytes);
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
    VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D,
    size_t bytes = 0,
    void* data = nullptr,
    bool mipmapped = false
);

void generate_mipmaps(
    VkCommandBuffer cmd,
    VkImage img,
    VkFormat format,
    uvec2 size,
    VkImageLayout before,
    VkImageLayout after
);

event copy_buffer(
    device& dev,
    VkBuffer dst_buffer,
    VkBuffer src_buffer,
    size_t bytes
);
vkres<VkBuffer> upload_buffer(
    device& dev,
    event& result_event,
    size_t bytes,
    const void* data,
    VkBufferUsageFlags usage
);

vkres<VkCommandBuffer> begin_command_buffer(device& dev);
event end_command_buffer(device& dev, VkCommandBuffer buf);

size_t calculate_descriptor_pool_sizes(
    size_t binding_count,
    VkDescriptorPoolSize* pool_sizes,
    const VkDescriptorSetLayoutBinding* bindings,
    uint32_t multiplier = 1
);

void image_barrier(
    VkCommandBuffer cmd,
    VkImage image,
    VkFormat format,
    VkImageLayout layout_before,
    VkImageLayout layout_after,
    uint32_t mip_level = 0,
    uint32_t mip_count = VK_REMAINING_MIP_LEVELS,
    uint32_t array_level = 0,
    uint32_t array_count = VK_REMAINING_ARRAY_LAYERS,
    VkAccessFlags2KHR happens_before = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR | VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
    VkAccessFlags2KHR happens_after = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR | VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR,
    VkPipelineStageFlags2KHR stage_before = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
    VkPipelineStageFlags2KHR stage_after = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR
);

void buffer_barrier(
    VkCommandBuffer cmd,
    VkBuffer buf,
    VkAccessFlags2KHR happens_before = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR | VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR,
    VkAccessFlags2KHR happens_after = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR | VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR,
    VkPipelineStageFlags2KHR stage_before = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
    VkPipelineStageFlags2KHR stage_after = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR
);

void full_barrier(VkCommandBuffer cmd);

void interlace(void* dst, const void* src, const void* fill, size_t src_stride, size_t dst_stride, size_t entries);

VkImageAspectFlags deduce_image_aspect_flags(VkFormat format);
VkImageUsageFlags deduce_image_usage_flags(
    VkFormat format, bool read, bool write
);


float half_to_float(uint16_t half);

size_t get_format_channels(VkFormat format);
size_t get_format_channel_size(VkFormat format, unsigned channel_index = 0);
size_t get_format_size(VkFormat format);
// Returns the single-channel format corresponding to the given multi-channel
// format.
VkFormat get_format_channel_type(VkFormat format);

// Here, simple means that it only has power-of-two members that are the same
// size and have no compression.
bool format_is_simple(VkFormat format);
bool format_is_depth(VkFormat format);
bool format_is_depth_stencil(VkFormat format);

template<typename T>
std::vector<T> read_formatted_data(size_t size, const void* data, VkFormat format);

#ifdef RAYBASE_HAS_SDL2
SDL_Surface* load_image(const char* path);
#endif
void write_image(std::string_view path, const void* data, VkFormat format, uvec2 size);

event async_save_image(
    device& dev, std::string path, VkImage image, VkImageLayout layout,
    VkFormat format, uvec2 size, argvec<event> wait_for, bool flip
);

}

#endif
