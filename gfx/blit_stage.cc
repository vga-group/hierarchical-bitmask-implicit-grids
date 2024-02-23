#include "blit_stage.hh"
#include "core/error.hh"

namespace rb::gfx
{

blit_stage::blit_stage(
    device& dev,
    render_target& src,
    video_output& output,
    VkFilter filter
):  render_stage(dev), vo(&output), src(src), filter(filter),
    stage_timer(dev, "blit")
{
    src.set_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}

void blit_stage::update_buffers(uint32_t frame_index)
{
    clear_commands();
    VkCommandBuffer cmd = graphics_commands(true);
    stage_timer.start(cmd, frame_index);

    uint32_t image_index = vo->get_image_index();
    src.transition_layout(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    render_target dst = vo->get_render_target(image_index);
    dst.transition_layout(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    RB_CHECK(
        src.get_samples() != VK_SAMPLE_COUNT_1_BIT,
        "You cannot blit MSAA buffers! Resolve them first into a different "
        "render target, then blit that."
    );

    if(src.get_size() == dst.get_size() && src.get_format() == dst.get_format())
    {
        VkImageCopy region = {
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            {0,0,0},
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            {0,0,0},
            {dst.get_size().x, dst.get_size().y, 1}
        };
        vkCmdCopyImage(
            cmd,
            src.get_image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dst.get_image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );
    }
    else
    {
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
            filter
        );
    }

    dst.transition_layout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, vo->get_output_layout());

    stage_timer.stop(cmd, frame_index);
    use_graphics_commands(cmd, frame_index);
}

}
