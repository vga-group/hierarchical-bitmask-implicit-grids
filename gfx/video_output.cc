#include "video_output.hh"
#include "vulkan_helpers.hh"

namespace rb::gfx
{

video_output::video_output(context& ctx)
: ctx(&ctx)
{
}

video_output::~video_output()
{
}

context& video_output::get_context() const
{
    return *ctx;
}

time_ticks video_output::get_frame_time() const
{
    return get_device().get_frame_time();
}

float video_output::get_aspect(size_t viewport) const
{
    vec2 sz = get_size(viewport);
    return sz.x/sz.y;
}

bool video_output::require_linear_colors() const
{
    return false;
}

void video_output::force_blit(render_target& src, argvec<event> wait_events)
{
    event swapchain_event;
    if(begin_frame(swapchain_event))
        return;

    uint32_t frame_index = get_device().get_in_flight_index();

    std::vector<event> events;
    events.assign(wait_events.begin(), wait_events.end());
    events.push_back(swapchain_event);

    vkres<VkCommandBuffer> cmd = begin_command_buffer(get_device());

    uint32_t image_index = get_image_index();
    src.transition_layout(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    render_target dst = get_render_target(image_index);
    dst.transition_layout(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageBlit region = {
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        {{0,0,0}, {(int)src.get_size().x, (int)src.get_size().y, 1}},
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        {{0,0,0}, {(int)dst.get_size().x, (int)dst.get_size().y, 1}}
    };
    vkCmdBlitImage(
        cmd,
        src.get_image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst.get_image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region,
        VK_FILTER_LINEAR
    );

    dst.transition_layout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, get_output_layout());

    vkEndCommandBuffer(cmd);
    event e = get_device().queue_graphics(events, *cmd, false);

    end_frame(e);
}

}

