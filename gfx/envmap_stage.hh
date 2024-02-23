#ifndef RAYBASE_GFX_ENVMAP_STAGE_HH
#define RAYBASE_GFX_ENVMAP_STAGE_HH

#include "device.hh"
#include "render_target.hh"
#include "render_stage.hh"
#include "raster_pipeline.hh"
#include "render_pass.hh"
#include "framebuffer.hh"
#include "sampler.hh"
#include "gpu_buffer.hh"
#include "timer.hh"
#include <vector>

namespace rb::gfx
{

class scene_stage;
class environment_map;

class envmap_stage: public render_stage
{
public:
    envmap_stage(
        scene_stage& scene,
        render_target& target,
        environment_map* force_envmap = nullptr
    );

    // Stenciled version
    envmap_stage(
        scene_stage& scene,
        render_target& color_target,
        render_target& stencil_target,
        uint32_t stencil_reference,
        environment_map* force_envmap = nullptr
    );

    void set_envmap(environment_map* force_envmap = nullptr);

protected:
    void update_buffers(uint32_t frame_index);

private:
    render_target target;
    environment_map* force_envmap;
    const texture* cubemap;
    scene_stage* scene_data;
    timer stage_timer;
    // A compute pipeline could also be used, but it's a tad more complicated
    // with MSAA.
    raster_pipeline pipeline;
    framebuffer fb;
    render_pass pass;
    descriptor_set descriptors;
    gpu_buffer parameters;
    sampler envmap_sampler;
};

}

#endif
