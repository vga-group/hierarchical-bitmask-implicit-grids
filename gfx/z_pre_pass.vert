#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_control_flow_attributes : enable

#define RAYBASE_SCENE_SET 0
#include "scene.glsl"

layout(location = 0) in vec3 in_pos;

layout(push_constant) uniform push_constant_buffer
{
    uint instance_index;
    uint camera_index;
} pc;

void main()
{
    const instance inst = instances.array[pc.instance_index];
    const camera cam = cameras.array[pc.camera_index];

    vec3 pos = (inst.model_to_world * vec4(in_pos, 1)).xyz;
    gl_Position = cam.view_proj * vec4(pos, 1);
}

