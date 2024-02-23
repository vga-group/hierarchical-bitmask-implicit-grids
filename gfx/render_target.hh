#ifndef RAYBASE_GFX_RENDER_TARGET_HH
#define RAYBASE_GFX_RENDER_TARGET_HH

#include "device.hh"
#include "core/math.hh"
#include "core/argvec.hh"
#include <vector>

namespace rb::gfx
{

class render_target
{
public:
    render_target();
    render_target(
        VkImage image,
        VkImageView view,
        VkImageLayout layout,
        uvec2 size,
        unsigned base_layer,
        unsigned layer_count,
        VkSampleCountFlagBits samples,
        VkFormat format
    );

    // You can use this to indicate layout change without actually recording it
    // now. You can use the image_barrier() command to do it later. The old
    // layout is returned.
    VkImageLayout set_layout(VkImageLayout layout);
    VkImageLayout get_layout() const;

    VkImage get_image() const;
    VkImageView get_view() const;

    // Note that this does not save the layout change to the render target, you
    // should also call set_layout() where relevant.
    void transition_layout(VkCommandBuffer buf, VkImageLayout to);
    void transition_layout(VkCommandBuffer buf, VkImageLayout from, VkImageLayout to);

    uvec2 get_size() const;
    VkSampleCountFlagBits get_samples() const;
    VkFormat get_format() const;
    bool is_depth() const;
    bool is_depth_stencil() const;
    unsigned get_layer_count() const;
    unsigned get_base_layer() const;

private:
    uvec2 size;
    unsigned base_layer;
    unsigned layer_count;
    VkSampleCountFlagBits samples;
    VkFormat format;
    VkImage image;
    VkImageView view;
    VkImageLayout layout;
};

}

#endif
