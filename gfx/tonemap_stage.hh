#ifndef RAYBASE_TONEMAP_STAGE_HH
#define RAYBASE_TONEMAP_STAGE_HH

#include "device.hh"
#include "compute_pipeline.hh"
#include "render_target.hh"
#include "render_stage.hh"
#include "gpu_buffer.hh"
#include "sampler.hh"
#include "timer.hh"
#include <vector>

namespace rb::gfx
{

enum class tonemap_operator: uint32_t
{
    NONE = 0, // No tonemapping, pass through as-is.
    REINHARD, // [0, inf] to [0, 1]
    REINHARD_EXTENDED, // [0, C] to [0, 1]
    REINHARD_JODIE, // [0, inf] to [0, 1]
    FILMIC_HABLE, // [0, inf] to [0, 1]
    FILMIC_ACES // [0, inf] to [0, 1]
};

enum class correction_type: uint32_t
{
    NONE = 0,
    GAMMA, // Gamma correction (gamma is adjustable)
    SRGB // sRGB correction (non-adjustable)
};

class texture;

// Does in-place tone mapping and color grading.
class tonemap_stage: public render_stage
{
public:
    struct options
    {
        // Color grading
        const texture* grading_lut = nullptr; // 3D lookup texture
        float gain = 1.0f;
        float saturation = 1.0f;
        mat3 color_transform = mat3(1);

        // Tone mapping
        tonemap_operator op = tonemap_operator::FILMIC_ACES;
        float max_white_luminance = 1.0f; // Only affects REINHARD_EXTENDED

        // Non-linearity correction
        correction_type correction = correction_type::SRGB;
        float gamma = 2.2f; // Only affects if correction == GAMMA
    };

    // dst is allowed to be the same as src. If the sample count differs, dst
    // must be VK_SAMPLE_COUNT_1_BIT (MSAA resolve will occur at the correct
    // point during tone mapping). Otherwise, the sample counts of src and dst
    // must be equal.
    tonemap_stage(
        device& dev,
        render_target& src,
        render_target& dst,
        const options& opt
    );

    // This can cause a re-recording of the command buffer for major changes.
    void set_options(const options& opt);

protected:
    void update_buffers(uint32_t frame_index) override;

private:
    bool need_record;
    uint32_t tile_size;
    options opt;
    render_target src, dst;
    compute_pipeline pipeline;
    descriptor_set descriptors;
    gpu_buffer parameters;
    sampler lut_sampler;
    timer stage_timer;
};

}

#endif
