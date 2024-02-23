#include "multiview_layout_stage.hh"
#include "multiview_layout_grid.comp.h"
#include "multiview_layout_blend.comp.h"

namespace
{

using namespace rb;
using namespace rb::gfx;

struct grid_push_constant_buffer
{
    pvec4 fill_color;
    uint32_t grid_width;
    uint32_t scale_to_fit;
};

}

namespace rb::gfx
{

multiview_layout_stage::multiview_layout_stage(
    device& dev,
    render_target& view_array,
    render_target& composed,
    const std::variant<grid, blend>& layout
):  render_stage(dev), layout_pipeline(dev), descriptors(dev),
    view_sampler(dev, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0),
    stage_timer(dev, "multiview layout"),
    layout(layout)
{
    shader_data shader;
    size_t pc_size = 0;
    if(grid* g = std::get_if<grid>(&this->layout))
    {
        if(g->grid_width == 0)
        {
            if(g->scale_to_fit)
            {
                for(;;)
                {
                    g->grid_width++;
                    int rows = (view_array.get_layer_count() + g->grid_width-1) / g->grid_width;
                    if(rows * composed.get_size().x * view_array.get_size().y <= composed.get_size().y * view_array.get_size().x * g->grid_width)
                        break;
                }
            }
            else
            {
                g->grid_width = view_array.get_size().x / composed.get_size().x;
            }
        }
        shader = multiview_layout_grid_comp_shader_binary;
        pc_size = sizeof(grid_push_constant_buffer);
    }
    else if(blend* b = std::get_if<blend>(&this->layout))
    {
        shader = multiview_layout_blend_comp_shader_binary;
    }

    descriptors.add(shader);
    layout_pipeline.init(shader, pc_size, descriptors.get_layout());

    for(uint32_t i = 0; i < dev.get_in_flight_count(); ++i)
    {
        VkCommandBuffer cmd = compute_commands();
        stage_timer.start(cmd, i);
        view_array.transition_layout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        composed.transition_layout(cmd, VK_IMAGE_LAYOUT_GENERAL);

        descriptors.set_image("src", view_array.get_view(), view_sampler.get());
        descriptors.set_image("dst", composed.get_view());

        layout_pipeline.bind(cmd);
        layout_pipeline.push_descriptors(cmd, descriptors);

        if(grid* g = std::get_if<grid>(&this->layout))
        {
            grid_push_constant_buffer pc;
            pc.grid_width = g->grid_width;
            pc.fill_color = g->empty_fill_color;
            pc.scale_to_fit = g->scale_to_fit ? 1 : 0;
            layout_pipeline.push_constants(cmd, &pc);
        }
        else if(blend* b = std::get_if<blend>(&this->layout))
        {
            //blend_push_constant_buffer pc;
            //layout_pipeline.push_constants(cmd, &pc);
        }
        layout_pipeline.dispatch(cmd, uvec3((composed.get_size()+15u)/16u, 1));

        view_array.transition_layout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

        stage_timer.stop(cmd, i);
        use_compute_commands(cmd, i);
    }

    view_array.set_layout(VK_IMAGE_LAYOUT_GENERAL);
    composed.set_layout(VK_IMAGE_LAYOUT_GENERAL);
}

}
