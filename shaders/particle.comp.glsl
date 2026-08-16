#version 450

#include "common/particle.glsl"

layout(local_size_x = 256) in;

layout(set = 0, binding = 0) buffer Particles
{
    Particle particles[];
};

layout(push_constant) uniform Push
{
    uint particleCount;
    float mouseX;
    float mouseY;
} push;

void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (id >= push.particleCount)
        return;

    Particle p = particles[id];

    vec2 pos = vec2(p.x, p.y);
    vec2 vel = vec2(p.vx, p.vy);
    vec2 mousePos = vec2(push.mouseX, push.mouseY);

    vec2 delta = mousePos - pos;

    float d = length(delta); 

    float influence = clamp(1.0 - d * 0.0045, 0.0, 1.0);
    float force = influence * 0.55 + 0.08;

    vec2 dVel = clamp(delta * (0.018 * force), vec2(-1.5), vec2(1.5));
    vel += dVel;

    float rotation = influence * 0.025 + 0.004;

    vec2 oldVel = vel;
    vel.x -= oldVel.y * rotation;
    vel.y += oldVel.x * rotation;

    float spreadX = fract(float(id) * 0.754877) - 0.5;
    float spreadY = fract(float(id) * 0.569841) - 0.5;

    vel += vec2(spreadX, spreadY) * 0.16;
    vel *= 0.992;

    float maxVelocity = 24.0 - influence * 5.0;
    
    vel = clamp(vel, vec2(-maxVelocity), vec2(maxVelocity));

    p.prevX = pos.x;
    p.prevY = pos.y;
    p.vx = vel.x;
    p.vy = vel.y;
    p.x = pos.x + vel.x;
    p.y = pos.y + vel.y;

    particles[id] = p;
}