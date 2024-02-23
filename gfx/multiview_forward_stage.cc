#include "multiview_forward_stage.hh"
#include "clustering_stage.hh"
#include "primitive.hh"
#include "light.hh"
#include "model.hh"
#include "vulkan_helpers.hh"
#include "multiview_forward.vert.h"
#include "multiview_forward.frag.h"
#include "multiview_z_pre_pass.vert.h"
#include "z_pre_pass.frag.h"

namespace
{
using namespace rb;
using namespace rb::gfx;

struct rasterizer_config
{
    pvec4 ambient;
    float directional_min_radius;
    float point_min_radius;
    float directional_pcf_bias;
    float point_pcf_bias;
};

struct push_constant_buffer
{
    uint32_t instance_index;
    uint32_t base_camera_index;
    float pad[2];
    rasterizer_config config;
};

void set_stencil_params(raster_pipeline::params& params, const multiview_forward_stage::options& opt)
{
    if(opt.enable_stencil)
    {
        params.depth_stencil_info.stencilTestEnable = VK_TRUE;
        VkStencilOpState state = {
            opt.fail_op,
            opt.pass_op,
            opt.fail_op,
            opt.compare_op,
            0xFFFFFFFF,
            0xFFFFFFFF,
            opt.stencil_reference
        };
        params.depth_stencil_info.front = state;
        params.depth_stencil_info.back = state;

        if(opt.use_material_stencil)
        {
            static constexpr VkDynamicState stencil_dynamic_states[] = {
                VK_DYNAMIC_STATE_STENCIL_REFERENCE
            };
            params.dynamic_info.dynamicStateCount = 1;
            params.dynamic_info.pDynamicStates = stencil_dynamic_states;
        }
    }
}

constexpr primitive::attribute_flag attributes =
        primitive::POSITION |
        primitive::NORMAL |
        primitive::TANGENT |
        primitive::TEXTURE_UV |
        primitive::LIGHTMAP_UV;
}

namespace rb::gfx
{

multiview_forward_stage::multiview_forward_stage(
    clustering_stage& clustering,
    render_target& color_buffer,
    render_target& depth_buffer,
    const options& opt
):  render_stage(clustering.get_device()),
    opt(opt),
    scene_data(clustering.get_scene_data()),
    pass(clustering.get_device()),
    z_pre_pass(clustering.get_device()),
    pipeline(clustering.get_device()),
    stage_timer(clustering.get_device(), "multiview forward pass")
{
    render_target* targets[] = {&color_buffer, &depth_buffer};

    this->opt.max_view_group_size = min(
        opt.max_view_group_size, min(
        clustering.get_device().vulkan11_props.maxMultiviewViewCount,
        color_buffer.get_layer_count()
    ));

    while((color_buffer.get_layer_count() % this->opt.max_view_group_size) != 0)
        this->opt.max_view_group_size--;

    std::vector<int32_t> view_offset(this->opt.max_view_group_size, 0);
    uint32_t view_mask = (1lu<<this->opt.max_view_group_size)-1;
    uint32_t view_masks[3] = {view_mask, view_mask, view_mask};

    {
        render_pass::params params;
        if(opt.z_pre_pass)
        {
            params = render_pass::params(targets, {
                render_pass::subpass{{1}},
                render_pass::subpass{{0,1}}
            });
            params.multi_view_info.subpassCount = 2;
            params.multi_view_info.dependencyCount = 3;
        }
        else
        {
            params = render_pass::params(targets, {});
            params.multi_view_info.subpassCount = 1;
            params.multi_view_info.dependencyCount = 2;
        }

        params.multi_view_info.pViewMasks = view_masks;
        params.multi_view_info.pViewOffsets = view_offset.data();
        params.multi_view_info.correlationMaskCount = 1;
        params.multi_view_info.pCorrelationMasks = &view_mask;
        pass.init(params);
    }

    {
        for(uint32_t i = 0; i < color_buffer.get_layer_count()/this->opt.max_view_group_size; ++i)
        {
            unsigned base_layer = i * this->opt.max_view_group_size;
            depth_group_views.push_back(create_image_view(
                clustering.get_device(), depth_buffer.get_image(), depth_buffer.get_format(),
                deduce_image_aspect_flags(depth_buffer.get_format()), VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                base_layer, this->opt.max_view_group_size, 0, 1
            ));
            color_group_views.push_back(create_image_view(
                clustering.get_device(), color_buffer.get_image(), color_buffer.get_format(),
                deduce_image_aspect_flags(color_buffer.get_format()), VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                base_layer, this->opt.max_view_group_size, 0, 1
            ));

            render_target depth_group_target(
                depth_buffer.get_image(),
                depth_group_views.back(),
                depth_buffer.get_layout(),
                depth_buffer.get_size(),
                base_layer,
                this->opt.max_view_group_size,
                depth_buffer.get_samples(),
                depth_buffer.get_format()
            );

            render_target color_group_target(
                color_buffer.get_image(),
                color_group_views.back(),
                color_buffer.get_layout(),
                color_buffer.get_size(),
                base_layer,
                this->opt.max_view_group_size,
                color_buffer.get_samples(),
                color_buffer.get_format()
            );

            framebuffer& fb = framebuffers.emplace_back();

            render_target* targets[] = {&color_group_target, &depth_group_target};
            fb.init(pass, targets);
        }
    }

    unsigned subpass_index = 0;

    if(opt.z_pre_pass)
    {
        auto bind_desc = primitive::get_bindings(primitive::POSITION);
        auto attr_desc = primitive::get_attributes(primitive::POSITION);

        raster_pipeline::params params(
            pass, subpass_index++,
            color_buffer.get_size(),
            bind_desc.size(),
            bind_desc.data(),
            attr_desc.size(),
            attr_desc.data()
        );
        set_stencil_params(params, opt);

        raster_shader_data shader;
        shader.vertex = multiview_z_pre_pass_vert_shader_binary;
        shader.fragment = z_pre_pass_frag_shader_binary;

        z_pre_pass.init(
            params, shader, sizeof(push_constant_buffer),
            scene_data->get_descriptor_set().get_layout()
        );
    }

    auto bind_desc = primitive::get_bindings(attributes);
    auto attr_desc = primitive::get_attributes(attributes);

    raster_pipeline::params params(
        pass, subpass_index++,
        color_buffer.get_size(),
        bind_desc.size(),
        bind_desc.data(),
        attr_desc.size(),
        attr_desc.data()
    );
    set_stencil_params(params, opt);

    raster_shader_data shader;
    shader.vertex = multiview_forward_vert_shader_binary;
    shader.fragment = multiview_forward_frag_shader_binary;
    clustering.get_specialization_info(shader.fragment.specialization);
    shader.fragment.specialization[11] = opt.dynamic_lighting ? 1 : 0;
    shader.fragment.specialization[12] = opt.alpha_discard ? 1 : 0;
    pipeline.init(
        params, shader, sizeof(push_constant_buffer),
        scene_data->get_descriptor_set().get_layout()
    );
}

void multiview_forward_stage::update_buffers(uint32_t frame_index)
{
    clear_commands();
    VkCommandBuffer cmd = graphics_commands(true);
    stage_timer.start(cmd, frame_index);

    const auto& cameras = scene_data->get_active_cameras();
    push_constant_buffer pc = {0, 0};
    scene* s = scene_data->get_scene();

    vec3 ambient = vec3(0);
    s->foreach([&](rendered&, ambient_light& al){
        ambient += al.color;
    });
    pc.config.ambient = vec4(ambient, 0);

    for(framebuffer& fb: framebuffers)
    {
        pass.begin(cmd, fb);

        if(opt.z_pre_pass)
        {
            z_pre_pass.bind(cmd);
            z_pre_pass.set_descriptors(cmd, scene_data->get_descriptor_set());

            for(auto& entry: scene_data->get_render_list())
            {
                if((entry.mask & opt.mask) == 0)
                    continue;
                pc.instance_index = entry.instance_index;
                z_pre_pass.push_constants(cmd, &pc);
                model& m = *s->get<model>(entry.id);
                const material& mat = m.materials[entry.vertex_group_index];

                if(opt.enable_stencil && opt.use_material_stencil)
                    vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, mat.stencil_reference);
                auto& vg = (*m.m)[entry.vertex_group_index];
                if(!mat.potentially_transparent())
                    vg.get_primitive()->draw(cmd, primitive::POSITION);
            }

            pass.next(cmd);
        }

        pipeline.bind(cmd);
        pipeline.set_descriptors(cmd, scene_data->get_descriptor_set());

        for(auto& entry: scene_data->get_render_list())
        {
            if((entry.mask & opt.mask) == 0)
                continue;

            pc.instance_index = entry.instance_index;
            pipeline.push_constants(cmd, &pc);
            model& m = *s->get<model>(entry.id);

            if(opt.enable_stencil && opt.use_material_stencil)
            {
                const material& mat = m.materials[entry.vertex_group_index];
                vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, mat.stencil_reference);
            }

            (*m.m)[entry.vertex_group_index].get_primitive()->draw(cmd, attributes);
        }

        pass.end(cmd);
        pc.base_camera_index += this->opt.max_view_group_size;
    }

    stage_timer.stop(cmd, frame_index);
    use_graphics_commands(cmd, frame_index);
}

}


