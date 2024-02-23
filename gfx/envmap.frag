#version 460
#extension GL_EXT_scalar_block_layout : enable

layout(binding = 0, scalar) uniform params_buffer
{
    mat4 transform;
    ivec2 screen_size;
} params;

layout(binding = 1) uniform samplerCube envmap;
layout(location = 0) out vec4 color;

void main()
{
    vec2 uv = vec2(gl_FragCoord.xy)/vec2(params.screen_size);
    uv.y = 1-uv.y;
    vec3 view_dir = normalize((params.transform * vec4(uv*2.0f-1.0f, 1, 1)).xyz);

    vec4 c = textureLod(envmap, view_dir, 0.0f);
    c.a = 1;
    color = c;
}
