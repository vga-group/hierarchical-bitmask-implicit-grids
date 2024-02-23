#include "tonemap_stage.hh"
#include "tonemap.comp.h"
#include "tonemap_resolve.comp.h"
#include "tonemap_msaa.comp.h"

namespace
{
using namespace rb::gfx;

struct push_constant_buffer
{
    rb::pivec2 size;
    uint32_t use_lookup_texture;
};

struct parameter_buffer
{
    rb::pmat4 color_transform;
    uint32_t tonemap_operator;
    uint32_t correction;
    float gain;
    float saturation;
    float max_white_luminance;
    float gamma;
};

}

namespace rb::gfx
{

tonemap_stage::tonemap_stage(
    device& dev,
    render_target& src,
    render_target& dst,
    const options& opt
):  render_stage(dev),
    need_record(true),
    opt(opt),
    src(src),
    dst(dst),
    pipeline(dev),
    descriptors(dev),
    parameters(dev, sizeof(parameter_buffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
    lut_sampler(dev, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, 0.0f),
    stage_timer(dev, "tonemap")
{
    shader_data shader;
    if(src.get_samples() == VK_SAMPLE_COUNT_1_BIT)
    {
        tile_size = 8;
        shader = tonemap_comp_shader_binary;
    }
    else if(dst.get_samples() == VK_SAMPLE_COUNT_1_BIT)
    {
        if(src.get_samples() > VK_SAMPLE_COUNT_8_BIT)
            tile_size = 4;
        else
            tile_size = 8;
        shader = tonemap_resolve_comp_shader_binary;
    }
    else
    {
        tile_size = 8;
        shader = tonemap_msaa_comp_shader_binary;
    }
    shader.specialization[0] = src.get_samples();
    shader.specialization[1] = tile_size;
    descriptors.add(shader);
    descriptors.set_binding_params("lut", 1, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
    pipeline.init(shader, sizeof(push_constant_buffer), descriptors.get_layout());

    src.set_layout(VK_IMAGE_LAYOUT_GENERAL);
    dst.set_layout(VK_IMAGE_LAYOUT_GENERAL);
}

void tonemap_stage::set_options(const options& opt)
{
    if(this->opt.grading_lut != opt.grading_lut)
        need_record = true;

    this->opt = opt;
}

void tonemap_stage::update_buffers(uint32_t frame_index)
{
    if(need_record)
    {
        clear_commands();
        descriptors.reset(1);
        descriptors.set_buffer(0, "parameters", (VkBuffer)parameters);
        descriptors.set_image(0, "src_target", src.get_view());
        descriptors.set_image(0, "dst_target", dst.get_view());
        if(opt.grading_lut)
            descriptors.set_texture(0, "lut", *opt.grading_lut, lut_sampler);

        for(uint32_t i = 0; i < dev->get_in_flight_count(); ++i)
        {
            VkCommandBuffer cmd = compute_commands();
            stage_timer.start(cmd, i);
            upload(cmd, &parameters, i);

            pipeline.bind(cmd);
            pipeline.set_descriptors(cmd, descriptors);
            src.transition_layout(cmd, VK_IMAGE_LAYOUT_GENERAL);
            if(src.get_image() != dst.get_image())
                dst.transition_layout(cmd, VK_IMAGE_LAYOUT_GENERAL);

            push_constant_buffer pc = {src.get_size(), opt.grading_lut != nullptr};
            pipeline.push_constants(cmd, &pc);
            pipeline.dispatch(cmd, (uvec3(src.get_size(), 1)+tile_size-1u)/tile_size);

            stage_timer.stop(cmd, i);
            use_compute_commands(cmd, i);
        }
        need_record = false;
    }

    parameter_buffer params;
    params.color_transform = pmat4(opt.color_transform);
    params.tonemap_operator = static_cast<uint32_t>(opt.op);
    params.correction = static_cast<uint32_t>(opt.correction);
    params.gain = opt.gain;
    params.saturation = opt.saturation;
    params.max_white_luminance = opt.max_white_luminance;
    params.gamma = 1.0f/opt.gamma;
    parameters.update(frame_index, params);
}

}

