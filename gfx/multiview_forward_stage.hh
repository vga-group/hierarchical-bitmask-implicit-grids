#ifndef RAYBASE_MULTIVIEW_FORWARD_STAGE_HH
#define RAYBASE_MULTIVIEW_FORWARD_STAGE_HH

#include "device.hh"
#include "render_target.hh"
#include "render_pass.hh"
#include "render_stage.hh"
#include "raster_pipeline.hh"
#include "framebuffer.hh"
#include "timer.hh"

namespace rb::gfx
{

class scene_stage;
class clustering_stage;

// Like forward_stage, but renders into a texture array instead. It pulls the 
class multiview_forward_stage: public render_stage
{
public:
    struct options
    {
        // Depending on GPU, this may or may not help with performance.
        bool z_pre_pass = true;

        // You can disable using all dynamic lights in the forward stage, to
        // fully rely on precalculated lighting instead. Your baked lighting
        // should then include direct lights as well.
        bool dynamic_lighting = true;

        // Only renders objects whose rg::rendered.mask & mask != 0.
        uint32_t mask = 0xFFFFFFFF;

        // Stencil settings.
        bool enable_stencil = false;
        bool use_material_stencil = true;
        bool alpha_discard = false;
        uint32_t stencil_reference = 0;
        VkCompareOp compare_op = VK_COMPARE_OP_ALWAYS;
        VkStencilOp fail_op = VK_STENCIL_OP_KEEP;
        VkStencilOp pass_op = VK_STENCIL_OP_REPLACE;

        // You may want to change how many views are rendered simultaneously due
        // to driver instabilities. 16 appears to be safe everywhere I've
        // tested.
        uint32_t max_view_group_size = 16;
    };

    multiview_forward_stage(
        clustering_stage& clustering,
        render_target& color_buffer,
        render_target& depth_buffer,
        const options& opt
    );

protected:
    void update_buffers(uint32_t frame_index) override;

private:
    options opt;
    scene_stage* scene_data;
    render_pass pass;
    std::vector<framebuffer> framebuffers;

    std::vector<vkres<VkImageView>> depth_group_views;
    std::vector<vkres<VkImageView>> color_group_views;

    raster_pipeline z_pre_pass, pipeline;
    timer stage_timer;
};

}

#endif

