#version 450

struct Particle
{
    float x;
    float y;

    float prevX;
    float prevY;

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

    float size;
    float trail_length;
    float trail_width;
} push;

layout(location = 0) out vec3 particleColor;
layout(location = 1) out float trailFactor;

void main()
{
    uint particleId = gl_VertexIndex / 6;
    uint vertexId = gl_VertexIndex % 6;

    Particle p = particles[particleId];

    vec2 position = vec2(p.x, p.y);
    vec2 previous = vec2(p.prevX, p.prevY);

    vec2 movement = position - previous;
    float movementLength = length(movement);

    vec2 direction;

    if (movementLength > 0.0001)
        direction = movement / movementLength;
    else
        direction = vec2(1.0, 0.0);

    vec2 perpendicular = vec2(-direction.y, direction.x);

    float halfWidth = push.trail_width * 0.5;

    float trailScale = clamp(
        movementLength / max(push.trail_length, 0.0001),
        0.15,
        1.0
    );

    vec2 tail = position - direction * push.trail_length * trailScale;

    vec2 positions[6] = vec2[](
        tail - perpendicular * halfWidth,
        tail + perpendicular * halfWidth,
        position - perpendicular * halfWidth,

        position - perpendicular * halfWidth,
        tail + perpendicular * halfWidth,
        position + perpendicular * halfWidth
    );

    float factors[6] = float[](
        0.0, 0.0, 1.0,
        1.0, 0.0, 1.0
    );

    vec2 finalPosition = positions[vertexId];

    trailFactor = factors[vertexId];

    float x = (finalPosition.x / push.width) * 2.0 - 1.0;
    float y = (finalPosition.y / push.height) * 2.0 - 1.0;

    gl_Position = vec4(x, y, 0.0, 1.0);

    uint r = p.color & 0xFFu;
    uint g = (p.color >> 8u) & 0xFFu;
    uint b = (p.color >> 16u) & 0xFFu;

    particleColor = vec3(r, g, b) / 255.0;
}