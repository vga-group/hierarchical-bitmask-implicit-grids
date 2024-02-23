#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_control_flow_attributes : enable

#define RAYBASE_SCENE_SET 0
#include "scene.glsl"

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_tangent;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec2 in_lightmap_uv;

layout(location = 0) out vec3 out_pos;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec3 out_tangent;
layout(location = 3) out vec3 out_bitangent;
layout(location = 4) out vec2 out_uv;
layout(location = 5) out vec2 out_lightmap_uv;

layout(push_constant) uniform push_constant_buffer
{
    uint instance_index;
    uint camera_index;
} pc;

void main()
{
    const instance inst = instances.array[pc.instance_index];
    const camera cam = cameras.array[pc.camera_index];

    out_pos = (inst.model_to_world * vec4(in_pos, 1)).xyz;
    gl_Position = cam.view_proj * vec4(out_pos, 1);
    out_uv = in_uv;
    out_lightmap_uv = in_lightmap_uv;
    out_normal = normalize(mat3(inst.normal_to_world) * in_normal);
    out_tangent = normalize(mat3(inst.model_to_world) * in_tangent.xyz);
    out_bitangent = cross(out_normal, out_tangent) * in_tangent.w;
}
