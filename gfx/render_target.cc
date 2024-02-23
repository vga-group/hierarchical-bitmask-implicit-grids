#include "render_target.hh"
#include "vulkan_helpers.hh"

namespace rb::gfx
{

render_target::render_target()
:   size(0), base_layer(0), layer_count(1), samples(VK_SAMPLE_COUNT_1_BIT),
    format(VK_FORMAT_R32G32B32A32_SFLOAT),
    image(VK_NULL_HANDLE),
    view(VK_NULL_HANDLE),
    layout(VK_IMAGE_LAYOUT_UNDEFINED)
{
}

render_target::render_target(
    VkImage image,
    VkImageView view,
    VkImageLayout layout,
    uvec2 size,
    unsigned base_layer,
    unsigned layer_count,
    VkSampleCountFlagBits samples,
    VkFormat format
):  size(size), base_layer(base_layer), layer_count(layer_count),
    samples(samples), format(format),
    image(image), view(view), layout(layout)
{
}

VkImageLayout render_target::set_layout(VkImageLayout layout)
{
    VkImageLayout old = this->layout;
    this->layout = layout;
    return old;
}

VkImageLayout render_target::get_layout() const
{
    return layout;
}

VkImage render_target::get_image() const
{
    return image;
}

VkImageView render_target::get_view() const
{
    return view;
}

void render_target::transition_layout(VkCommandBuffer buf, VkImageLayout to)
{
    transition_layout(buf, layout, to);
}

void render_target::transition_layout(VkCommandBuffer buf, VkImageLayout from, VkImageLayout to)
{
    if(from == to || image == VK_NULL_HANDLE)
        return;

    image_barrier(buf, image, format, from, to, 0, VK_REMAINING_MIP_LEVELS, base_layer, layer_count);
}

uvec2 render_target::get_size() const
{
    return size;
}

VkSampleCountFlagBits render_target::get_samples() const
{
    return samples;
}

VkFormat render_target::get_format() const
{
    return format;
}

bool render_target::is_depth() const
{
    return format_is_depth(format);
}

bool render_target::is_depth_stencil() const
{
    return format_is_depth_stencil(format);
}

unsigned render_target::get_layer_count() const
{
    return layer_count;
}

unsigned render_target::get_base_layer() const
{
    return base_layer;
}

}
