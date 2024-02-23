#ifndef RAYBASE_BLIT_STAGE_HH
#define RAYBASE_BLIT_STAGE_HH

#include "device.hh"
#include "render_target.hh"
#include "render_stage.hh"
#include "video_output.hh"
#include "timer.hh"

namespace rb::gfx
{

// Mostly useful for blitting in-flight frames to swapchain images, as this
// thing handles the multiple indices correctly.
class blit_stage: public render_stage
{
public:
    blit_stage(
        device& dev,
        render_target& src,
        video_output& output,
        VkFilter filter = VK_FILTER_LINEAR
    );

protected:
    void update_buffers(uint32_t frame_index) override;

private:
    video_output* vo;
    render_target src;
    VkFilter filter;
    timer stage_timer;
};

}

#endif
