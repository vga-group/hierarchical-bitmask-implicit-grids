#version 460

vec2 pos[3] = vec2[](
    vec2(-1, -1), vec2(4, -1), vec2(-1, 4)
);

void main()
{
    vec2 p = pos[gl_VertexIndex];
    gl_Position = vec4(p, 0.0f, 1.0f);
}
