#version 450

struct Particle
{
    float x;
    float y;
    float vx;
    float vy;
    uint color;
};

layout(set = 0, binding = 0) readonly buffer ParticleBuffer
{
    Particle particles[];
};

layout(push_constant) uniform Push
{
    float width;
    float height;
} push;

layout(location = 0) out vec3 particleColor;

void main()
{
    uint id = gl_VertexIndex;
    Particle p = particles[id];

    float x = (p.x / push.width) * 2.0 - 1.0;
    float y = (p.y / push.height) * 2.0 - 1.0;

    gl_Position = vec4(x, y, 0.0, 1.0);
    gl_PointSize = 1.0;

    uint r = p.color & 0xFFu;
    uint g = (p.color >> 8u) & 0xFFu;
    uint b = (p.color >> 16u) & 0xFFu;

    particleColor = vec3(r, g, b) / 255.0;
}