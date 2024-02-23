#include "clear_stage.hh"

namespace rb::gfx
{

clear_stage::clear_stage(
    device& dev,
    const std::vector<clear_info>& targets
): render_stage(dev), stage_timer(dev, "clear")
{
    for(uint32_t i = 0; i < dev.get_in_flight_count(); ++i)
    {
        VkCommandBuffer cmd = graphics_commands();
        stage_timer.start(cmd, i);
        for(auto info: targets)
        {
            VkImageSubresourceRange range = {
                0, 0, 1, 0, 1
            };
            info.target->transition_layout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            if(info.target->is_depth())
            {
                range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                if(info.target->is_depth_stencil())
                    range.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                if(info.aspect_mask != 0) range.aspectMask = info.aspect_mask;

                vkCmdClearDepthStencilImage(
                    cmd,
                    info.target->get_image(),
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    &info.value.depthStencil,
                    1,
                    &range
                );
                dev.gc.depend(info.target->get_image(), cmd);
            }
            else
            {
                range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                vkCmdClearColorImage(
                    cmd,
                    info.target->get_image(),
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    &info.value.color,
                    1,
                    &range
                );
                dev.gc.depend(info.target->get_image(), cmd);
            }
        }
        stage_timer.stop(cmd, i);
        use_graphics_commands(cmd, i);
    }

    for(auto info: targets)
        info.target->set_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

}
