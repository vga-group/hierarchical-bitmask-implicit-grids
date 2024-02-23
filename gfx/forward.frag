#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_control_flow_attributes : enable
#extension GL_KHR_shader_subgroup_arithmetic : enable

#include "forward.glsl"

layout(push_constant) uniform push_constant_buffer
{
    uint instance_index;
    uint camera_index;
    float pad[2];
    rasterizer_config config;
} pc;

layout(location = 0) out vec4 out_color;

void main()
{
    instance inst = instances.array[pc.instance_index];

    vertex_data vd;
    material mat;
    get_surface_info(inst, vd, mat);

    if(RB_ALPHA_DISCARD == 1)
    {
        if(mat.albedo.a == 0.0f)
            discard;
    }

    camera cam = cameras.array[pc.camera_index];
    vec3 view = normalize(get_camera_origin(cam) - vd.pos);

    out_color = eval_light_contribution(inst, view, vd, mat, pc.config);
}

