#version 450

#include "common/particle.glsl"
#include "common/particle_push.glsl"

layout(set = 0, binding = 0) readonly buffer ParticleBuffer
{
    Particle particles[];
};

layout(location = 0) flat out vec3 particleColor;
layout(location = 1) out vec2 localPos;
layout(location = 2) flat out vec4 trailData;

const vec2 POS_QUAD[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
);

void main()
{
    uint pId = gl_VertexIndex / 6u;
    Particle p = particles[pId];

    vec2 pos = vec2(p.x, p.y);
    vec2 move = pos - vec2(p.prevX, p.prevY);

    float mag = length(move);
    vec2 dir = mag > 0.001 ? (move / mag) : vec2(0.0);

    float halfW = max(push.trail_width, push.size) * 0.5;
    float minTrail = max(halfW * 8.0, push.size * 4.0);

    float trail = clamp(mag * 16.0 + minTrail, minTrail, push.trail_length);
    float L = max(trail / halfW, 8.0);

    uint v = gl_VertexIndex - pId * 6u;
    vec2 q = POS_QUAD[v];

    float x = q.x > 0.0 ? 1.5 : (-L - 1.5);

    vec2 side = vec2(-dir.y, dir.x) * halfW;
    vec2 finalPos = pos + dir * (halfW * x) + side * q.y;

    gl_Position = vec4(
        finalPos * (2.0 / vec2(push.width, push.height)) - 1.0,
        0.0,
        1.0
    );

    localPos = vec2(x, q.y);

    trailData = vec4(
        L,
        halfW,
        1.0 / L,
        1.0 / max(halfW * 0.35, 0.75)
    );

    particleColor = unpackUnorm4x8(p.color).rgb;
}