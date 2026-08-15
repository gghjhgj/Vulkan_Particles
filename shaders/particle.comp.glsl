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

uint hashUint(uint x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;

    return x;
}

float random01(uint seed)
{
    return float(hashUint(seed)) * 2.3283064365386963e-10;
}

vec3 random3(uint seed)
{
    return vec3(
        random01(seed * 0x9E3779B9u + 0x68BC21EBu),
        random01(seed * 0x85EBCA6Bu + 0x02E5BE93u),
        random01(seed * 0xC2B2AE35u + 0x27D4EB2Fu)
    );
}

vec2 flowField(float x, float y)
{
    float sx = sin(x * 0.010);
    float cx = cos(x * 0.010);

    float sy = sin(y * 0.014);
    float cy = cos(y * 0.014);

    float ax = x * 0.011 + sy * 2.1;
    float ay = y * 0.013 + cx * 1.8;

    float sinAx = sin(ax);
    float cosAx = cos(ax);

    float sinAy = sin(ay);
    float cosAy = cos(ay);

    float xy = (x + y) * 0.007;
    float xd = (x - y) * 0.009;

    float cosXY = cos(xy);
    float sinXD = sin(xd);

    float dA_dx = cosAx * 0.011;
    float dA_dy = cosAx * cy * 0.0294;

    float dB_dx = -cosAy * sx * 0.018;
    float dB_dy = cosAy * 0.013;

    float dC_dx = cosXY * 0.007;
    float dC_dy = cosXY * 0.007;

    float dD_dx = -sinXD * 0.009;
    float dD_dy =  sinXD * 0.009;

    float dFdx =
        dA_dx * 0.45 +
        dB_dx * 0.35 +
        dC_dx * 0.20 +
        dD_dx * 0.15;

    float dFdy =
        dA_dy * 0.45 +
        dB_dy * 0.35 +
        dC_dy * 0.20 +
        dD_dy * 0.15;

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

    vec3 rnd = random3(id);

    float randomX = rnd.x * 2.0 - 1.0;
    float randomY = rnd.y * 2.0 - 1.0;
    float radialRandom = rnd.z * 2.0 - 1.0;

    float dx = p.x - push.mouseX;
    float dy = p.y - push.mouseY;

    float distSq =
        dx * dx +
        dy * dy;

    float safeDistSq =
        max(distSq, 0.000001);

    float invDist =
        inversesqrt(safeDistSq);

    float dist =
        distSq * invDist;

    float nx =
        dx * invDist;

    float ny =
        dy * invDist;

    float radius = 180.0;

    float outside =
        max(dist - radius, 0.0);

    float boundaryForce =
        outside * 0.0035;

    p.vx -=
        nx * boundaryForce;

    p.vy -=
        ny * boundaryForce;

    float centerForce =
        max(8.0 - dist, 0.0) * 0.003;

    p.vx +=
        nx * centerForce;

    p.vy +=
        ny * centerForce;

    vec2 flow =
        flowField(p.x, p.y);

    float flowSq =
        dot(flow, flow);

    float invFlowLength =
        inversesqrt(
            max(flowSq, 0.000001)
        );

    flow *= invFlowLength;

    float flowStrength = 0.13;

    p.vx +=
        flow.x * flowStrength;

    p.vy +=
        flow.y * flowStrength;

    float dispersion = 0.11;

    p.vx +=
        randomX * dispersion;

    p.vy +=
        randomY * dispersion;

    float sideStrength = 0.055;

    p.vx +=
        -flow.y * sideStrength;

    p.vy +=
         flow.x * sideStrength;

    float mouseInfluence =
        clamp(
            1.0 - dist / 220.0,
            0.0,
            1.0
        );

    float mouseCurl = 0.035;

    p.vx +=
        -ny *
        mouseCurl *
        mouseInfluence;

    p.vy +=
         nx *
         mouseCurl *
         mouseInfluence;

    float radialStrength = 0.035;

    p.vx +=
        nx *
        radialRandom *
        radialStrength *
        mouseInfluence;

    p.vy +=
        ny *
        radialRandom *
        radialStrength *
        mouseInfluence;

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

    float invSpeed =
        inversesqrt(
            max(speedSq, 0.000001)
        );

    float speed =
        speedSq * invSpeed;

    float maxSpeed =
        mix(
            10.0,
            7.5,
            mouseInfluence
        );

    float speedScale =
        min(
            1.0,
            maxSpeed * invSpeed
        );

    p.vx *= speedScale;
    p.vy *= speedScale;

    p.x += p.vx;
    p.y += p.vy;

    particles[id] = p;
}