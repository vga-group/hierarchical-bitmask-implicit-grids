#include "path_tracer_stage.hh"
#include "clustering_stage.hh"
#include "path_tracer.rgen.h"
#include "vulkan_helpers.hh"

namespace
{
using namespace rb;

struct push_constant_buffer
{
    rb::gfx::gpu_path_tracer_config config;
    uint32_t sample_index;
    uint32_t camera_index;
    float blend_ratio;
};

static_assert(sizeof(push_constant_buffer) <= 128, "Push constant min-max size is 128!");

}

namespace rb::gfx
{

path_tracer_stage::path_tracer_stage(
    clustering_stage& clustering,
    render_target& output,
    const options& opt
):  render_stage(clustering.get_device()),
    pt_core(clustering),
    set(clustering.get_device()),
    output(output),
    opt(opt),
    stage_timer(clustering.get_device(), "path tracer"),
    accumulated_samples(0), sample_counter(0)
{
    set.add(path_tracer_rgen_shader_binary);
    set.reset(1);
    set.set_image(0, "color_target", output.get_view());

    pt_core.init(
        path_tracer_rgen_shader_binary,
        sizeof(push_constant_buffer),
        set.get_layout(),
        opt
    );

    output.set_layout(VK_IMAGE_LAYOUT_GENERAL);
}

void path_tracer_stage::reset_accumulation()
{
    accumulated_samples = 0;
}

void path_tracer_stage::update_buffers(uint32_t frame_index)
{
    if(!opt.accumulate)
        accumulated_samples = 0;

    push_constant_buffer pc;
    pc.camera_index = 0;

    clear_commands();
    VkCommandBuffer cmd = graphics_commands(true);
    output.transition_layout(cmd, VK_IMAGE_LAYOUT_GENERAL);
    stage_timer.start(cmd, frame_index);

    pc.config = pt_core.bind(cmd);
    pt_core.pipeline.set_descriptors(cmd, set, 0, 0);

    for(
        unsigned i = 0;
        i < opt.samples_per_pixel;
        ++i, ++accumulated_samples, ++sample_counter
    ){
        if(i != 0)
        {
            image_barrier(
                cmd,
                output.get_image(),
                output.get_format(),
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                0, 1,
                0, VK_REMAINING_ARRAY_LAYERS,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR
            );
        }
        pc.blend_ratio = 1.0f / (1.0f + accumulated_samples);
        pc.sample_index = sample_counter;

        pt_core.pipeline.push_constants(cmd, &pc);
        pt_core.pipeline.trace_rays(cmd, uvec3(output.get_size(), 1));
    }

    stage_timer.stop(cmd, frame_index);
    use_graphics_commands(cmd, frame_index);
}

}
