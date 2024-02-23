#ifndef RAYBASE_MULTIVIEW_LAYOUT_STAGE_HH
#define RAYBASE_MULTIVIEW_LAYOUT_STAGE_HH

#include "device.hh"
#include "render_target.hh"
#include "render_stage.hh"
#include "compute_pipeline.hh"
#include "timer.hh"
#include "sampler.hh"
#include <variant>

namespace rb::gfx
{

// Lays out multiview images into a single output image.
class multiview_layout_stage: public render_stage
{
public:
    struct grid
    {
        uint32_t grid_width = 0; // 0 is auto-deduce!
        vec4 empty_fill_color = vec4(0);
        bool scale_to_fit = false;
    };

    struct blend
    {
    };

    multiview_layout_stage(
        device& dev,
        render_target& view_array,
        render_target& composed,
        const std::variant<grid, blend>& layout
    );

private:
    std::variant<grid, blend> layout;
    compute_pipeline layout_pipeline;
    push_descriptor_set descriptors;
    sampler view_sampler;
    timer stage_timer;
};

}

#endif
