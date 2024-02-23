#ifndef RAYBASE_GFX_CLEAR_STAGE_HH
#define RAYBASE_GFX_CLEAR_STAGE_HH

#include "device.hh"
#include "render_target.hh"
#include "render_stage.hh"
#include "timer.hh"
#include <vector>

namespace rb::gfx
{

class clear_stage: public render_stage
{
public:
    struct clear_info
    {
        render_target* target = nullptr;
        VkClearValue value;
        // 0 gets auto-filled. Needed if you want to clear only depth or only
        // stencil from a combined depth-stencil buffer.
        VkImageAspectFlags aspect_mask = 0;
    };

    clear_stage(
        device& dev,
        const std::vector<clear_info>& targets
    );
private:
    timer stage_timer;
};

}

#endif
