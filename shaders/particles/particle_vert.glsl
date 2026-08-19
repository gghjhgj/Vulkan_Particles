#version 450
#extension GL_GOOGLE_include_directive : require

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
    
    vec2 dir = mag > 0.05 ? (move / mag) : vec2(1.0, 0.0);

    float halfW = max(push.trail_width, push.size) * 0.5;

    float trail = clamp(mag * 12.0, 0.0, push.trail_length);
    float L = trail / halfW;
    float effectiveL = max(L, 0.1);

    uint v = gl_VertexIndex - pId * 6u;
    vec2 q = POS_QUAD[v];

    float x = q.x > 0.0 ? 1.5 : (-effectiveL - 1.5);

    vec2 side = vec2(-dir.y, dir.x) * halfW;
    vec2 finalPos = pos + dir * (halfW * x) + side * q.y;

    gl_Position = vec4(finalPos * (2.0 / vec2(push.width, push.height)) - 1.0, 0.0, 1.0);

    localPos = vec2(x, q.y);

    trailData = vec4(
        effectiveL,
        halfW,
        1.0 / effectiveL,
        1.0 / max(halfW * 0.35, 0.75)
    );

    particleColor = unpackUnorm4x8(p.color).rgb;
}