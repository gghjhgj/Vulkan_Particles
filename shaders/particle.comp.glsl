#version 450

layout(local_size_x = 256) in;

struct Particle
{
    float x;
    float y;
    float vx;
    float vy;
    uint color;
};

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

float hash(float n)
{
    return fract(sin(n * 127.1) * 43758.5453);
}

float field(float x, float y)
{
    float a = sin(
        x * 0.011 +
        sin(y * 0.014) * 2.1
    );

    float b = sin(
        y * 0.013 +
        cos(x * 0.010) * 1.8
    );

    float c = sin(
        (x + y) * 0.007
    );

    float d = cos(
        (x - y) * 0.009
    );

    return
        a * 0.45 +
        b * 0.35 +
        c * 0.20 +
        d * 0.15;
}

vec2 flowField(float x, float y)
{
    float eps = 1.0;

    float left  = field(x - eps, y);
    float right = field(x + eps, y);

    float down  = field(x, y - eps);
    float up    = field(x, y + eps);

    float dFdx = (right - left) * 0.5;
    float dFdy = (up - down) * 0.5;

    return vec2(
        dFdy,
        -dFdx
    );
}

void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (id >= push.particleCount)
        return;

    Particle p = particles[id];

    float seed = float(id);

    float n1 = hash(seed * 1.173);
    float n2 = hash(seed * 2.731);
    float n3 = hash(seed * 4.217);
    float n4 = hash(seed * 7.913);

    float dx = p.x - push.mouseX;
    float dy = p.y - push.mouseY;

    float distSq =
        dx * dx +
        dy * dy;

    float dist = sqrt(distSq);

    float radius = 180.0;

    if (dist > 0.001)
    {
        float nx = dx / dist;
        float ny = dy / dist;

        if (dist > radius)
        {
            float outside = dist - radius;

            float boundaryForce = outside * 0.0035;

            p.vx -= nx * boundaryForce;
            p.vy -= ny * boundaryForce;
        }

        if (dist < 8.0)
        {
            float centerForce =
                (8.0 - dist) * 0.003;

            p.vx += nx * centerForce;
            p.vy += ny * centerForce;
        }
    }

    vec2 flow = flowField(p.x, p.y);

    float flowLength = length(flow);

    if (flowLength > 0.000001)
    {
        flow /= flowLength;
    }

    float flowStrength = 0.13;

    p.vx += flow.x * flowStrength;
    p.vy += flow.y * flowStrength;

    float randomX =
        n2 * 2.0 - 1.0;

    float randomY =
        n3 * 2.0 - 1.0;

    float dispersion = 0.11;

    p.vx += randomX * dispersion;
    p.vy += randomY * dispersion;

    float sideStrength = 0.055;

    p.vx += -flow.y * sideStrength;
    p.vy +=  flow.x * sideStrength;

    float mouseInfluence =
        clamp(
            1.0 - dist / 220.0,
            0.0,
            1.0
        );

    if (dist > 0.001)
    {
        float nx = dx / dist;
        float ny = dy / dist;

        float mouseCurl = 0.035;

        p.vx += -ny * mouseCurl * mouseInfluence;
        p.vy +=  nx * mouseCurl * mouseInfluence;
    }

    if (dist > 0.001)
    {
        float nx = dx / dist;
        float ny = dy / dist;

        float radialRandom =
            n4 * 2.0 - 1.0;

        float radialStrength = 0.035;

        p.vx += nx *
                radialRandom *
                radialStrength *
                mouseInfluence;

        p.vy += ny *
                radialRandom *
                radialStrength *
                mouseInfluence;
    }

    float damping =
        mix(
            0.993,
            0.985,
            mouseInfluence
        );

    p.vx *= damping;
    p.vy *= damping;

    float speedSq =
        p.vx * p.vx +
        p.vy * p.vy;

    float speed = sqrt(speedSq);

    float maxSpeed =
        mix(
            10.0,
            7.5,
            mouseInfluence
        );

    if (speed > maxSpeed)
    {
        float scale =
            maxSpeed / speed;

        p.vx *= scale;
        p.vy *= scale;
    }

    p.x += p.vx;
    p.y += p.vy;

    particles[id] = p;
}