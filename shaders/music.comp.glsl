#version 450

struct Particle
{
    float x;
    float y;
    float vx;
    float vy;
    uint color;
};

layout(local_size_x = 256) in;

layout(set = 0, binding = 0) buffer Particles
{
    Particle particles[];
};

layout(push_constant) uniform Push
{
    uint particleCount;
};

void main()
{
    uint index = gl_GlobalInvocationID.x;

    if (index >= particleCount)
        return;

    Particle p = particles[index];

    particles[index] = p;
}