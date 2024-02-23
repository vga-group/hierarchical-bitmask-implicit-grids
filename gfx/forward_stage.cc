#include "forward_stage.hh"
#include "clustering_stage.hh"
#include "primitive.hh"
#include "light.hh"
#include "model.hh"
#include "forward.vert.h"
#include "forward.frag.h"
#include "z_pre_pass.vert.h"
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
    uint32_t camera_index;
    float pad[2];
    rasterizer_config config;
};

void set_stencil_params(raster_pipeline::params& params, const forward_stage::options& opt)
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

forward_stage::forward_stage(
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
    stage_timer(clustering.get_device(), "forward pass")
{
    render_target* targets[] = {&color_buffer, &depth_buffer};
    if(opt.z_pre_pass)
    {
        pass.init(render_pass::params(targets, {
            render_pass::subpass{{1}},
            render_pass::subpass{{0,1}}
        }));
    }
    else
    {
        pass.init(render_pass::params(targets, {}));
    }
    fb.init(pass, targets);

    unsigned subpass_index = 0;

    if(opt.z_pre_pass)
    {
        auto bind_desc = primitive::get_bindings(primitive::POSITION);
        auto attr_desc = primitive::get_attributes(primitive::POSITION);

        raster_pipeline::params params(
            pass, subpass_index++,
            fb.get_size(),
            bind_desc.size(),
            bind_desc.data(),
            attr_desc.size(),
            attr_desc.data()
        );
        set_stencil_params(params, opt);

        raster_shader_data shader;
        shader.vertex = z_pre_pass_vert_shader_binary;
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
        fb.get_size(),
        bind_desc.size(),
        bind_desc.data(),
        attr_desc.size(),
        attr_desc.data()
    );
    set_stencil_params(params, opt);

    raster_shader_data shader;
    shader.vertex = forward_vert_shader_binary;
    shader.fragment = forward_frag_shader_binary;
    clustering.get_specialization_info(shader.fragment.specialization);
    shader.fragment.specialization[11] = opt.dynamic_lighting ? 1 : 0;
    shader.fragment.specialization[12] = opt.alpha_discard ? 1 : 0;
    pipeline.init(
        params, shader, sizeof(push_constant_buffer),
        scene_data->get_descriptor_set().get_layout()
    );
}

void forward_stage::update_buffers(uint32_t frame_index)
{
    clear_commands();
    VkCommandBuffer cmd = graphics_commands(true);
    stage_timer.start(cmd, frame_index);

    const auto& cameras = scene_data->get_active_cameras();
    entity camera_id = cameras[0];
    push_constant_buffer pc = {0, 0};
    scene* s = scene_data->get_scene();

    vec3 ambient = vec3(0);
    s->foreach([&](rendered&, ambient_light& al){
        ambient += al.color;
    });
    pc.config.ambient = vec4(ambient, 0);

    pass.begin(cmd, fb);

    if(opt.z_pre_pass)
    {
        z_pre_pass.bind(cmd);
        z_pre_pass.set_descriptors(cmd, scene_data->get_descriptor_set());

        for(auto& entry: scene_data->get_render_list(camera_id))
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

    for(auto& entry: scene_data->get_render_list(camera_id))
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
    stage_timer.stop(cmd, frame_index);
    use_graphics_commands(cmd, frame_index);
}

}

