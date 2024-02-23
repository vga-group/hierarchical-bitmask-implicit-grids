#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_control_flow_attributes : enable

#include "rt_generic.glsl"

layout(location = 0) rayPayloadInEXT bounce_payload payload;

void main()
{
    payload.primitive_index = -1;
    payload.instance_index = -1;
    payload.hit_attribs = vec2(0);
}

