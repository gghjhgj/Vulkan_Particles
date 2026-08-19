#version 450

layout(location = 0) out vec2 outUV;

const vec2 UVS[3] = vec2[](
    vec2(0.0, 0.0),
    vec2(0.0, 2.0),
    vec2(2.0, 0.0)
);

void main()
{
    outUV = UVS[gl_VertexIndex];
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
}