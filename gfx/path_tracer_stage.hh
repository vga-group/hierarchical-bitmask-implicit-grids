#ifndef RAYBASE_PATH_TRACER_STAGE_HH
#define RAYBASE_PATH_TRACER_STAGE_HH

#include "render_target.hh"
#include "render_stage.hh"
#include "timer.hh"
#include "path_tracer.hh"

namespace rb::gfx
{

class clustering_stage;

class path_tracer_stage: public render_stage
{
public:
    struct options: path_tracer::options
    {
        unsigned samples_per_pixel = 1;
        bool accumulate = true;
    };

    path_tracer_stage(
        clustering_stage& clustering,
        render_target& output,
        const options& opt
    );

    void reset_accumulation();

protected:
    void update_buffers(uint32_t frame_index) override;

private:
    path_tracer pt_core;
    descriptor_set set;
    render_target output;
    options opt;
    timer stage_timer;
    unsigned accumulated_samples;
    unsigned sample_counter;
};

}

#endif

