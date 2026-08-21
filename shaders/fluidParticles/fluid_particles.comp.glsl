#version 450

layout(local_size_x = 256) in;

layout(constant_id = 0) const uint WORKGROUP_SIZE = 256;
layout(constant_id = 1) const uint PARTICLE_COUNT = 0;

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

// Binding 0: Cząsteczki (SSBO)
layout(std430, set = 0, binding = 0) buffer ParticleBuffer
{
    Particle particles[];
};

// Binding 1 i 2: Tekstury 2D Płynu (Storage Images Read/Write)
layout(rg32f, set = 0, binding = 1) uniform image2D inOutVelocity;
layout(rgba32f, set = 0, binding = 2) uniform image2D inOutColor;

layout(push_constant) uniform Push
{
    float mouseX;
    float mouseY;
    float prevMouseX;
    float prevMouseY;
    float dt;
    float splatRadius;
    float splatForce;
    float velocityDissipation;
    float densityDissipation;
    float vorticity;
    uint simWidth;
    uint simHeight;
    uint windowWidth;
    uint windowHeight;
    uint isMouseDown;
} push;

vec2 sampleFluidVelocity(vec2 uv)
{
    vec2 pos = uv * vec2(float(push.simWidth), float(push.simHeight)) - 0.5;

    int maxW = int(push.simWidth) - 1;
    int maxH = int(push.simHeight) - 1;

    ivec2 i0 = clamp(ivec2(floor(pos)), ivec2(0), ivec2(maxW, maxH));
    ivec2 i1 = clamp(i0 + 1,            ivec2(0), ivec2(maxW, maxH));
    vec2 f = fract(pos);

    vec2 v00 = imageLoad(inOutVelocity, ivec2(i0.x, i0.y)).xy;
    vec2 v10 = imageLoad(inOutVelocity, ivec2(i1.x, i0.y)).xy;
    vec2 v01 = imageLoad(inOutVelocity, ivec2(i0.x, i1.y)).xy;
    vec2 v11 = imageLoad(inOutVelocity, ivec2(i1.x, i1.y)).xy;

    return mix(mix(v00, v10, f.x), mix(v01, v11, f.x), f.y);
}

vec3 getVelocityColor(vec2 dir)
{
    float len = length(dir);
    if (len < 0.001) return vec3(0.0, 0.8, 1.0);
    float angle = atan(dir.y, dir.x);
    return 0.5 + 0.5 * cos(angle + vec3(0.0, 2.0, 4.0));
}

float distToSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a;
    vec2 ba = b - a;
    float d = dot(ba, ba);
    float h = (d > 0.00001) ? clamp(dot(pa, ba) / d, 0.0, 1.0) : 0.0;
    return length(pa - ba * h);
}

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= PARTICLE_COUNT)
        return;

    Particle p = particles[id];

    vec2 screenRes = vec2(
        push.windowWidth > 0 ? float(push.windowWidth) : 1920.0,
        push.windowHeight > 0 ? float(push.windowHeight) : 1080.0
    );

    if (isnan(p.x) || isnan(p.vx))
    {
        p.x = screenRes.x * 0.5;
        p.y = screenRes.y * 0.5;
        p.vx = 0.0;
        p.vy = 0.0;
    }

    vec2 simRes = vec2(float(push.simWidth), float(push.simHeight));
    vec2 pos = vec2(p.x, p.y);
    vec2 vel = vec2(p.vx, p.vy);

    float dt = (push.dt > 0.0 && push.dt < 0.1) ? push.dt : 0.016;

    vec2 uv = pos / screenRes;
    vec2 fluidVel = sampleFluidVelocity(uv);

    vec2 fluidVelPixels = fluidVel * (screenRes.x / simRes.x) * 15.0;
    
    vel = mix(vel, fluidVelPixels, clamp(6.0 * dt, 0.0, 1.0));

    if (push.isMouseDown != 0)
    {
        vec2 mousePos = vec2(push.mouseX, push.mouseY);

        vec2 delta = pos - mousePos;
        float dist = length(delta);

        const float RADIUS = 700.0;
        const float MIN_RADIUS = 60.0;

        if (dist > 0.001 && dist < RADIUS)
        {
            float orbitRadius = clamp(dist, MIN_RADIUS, RADIUS);
            float angle = atan(delta.y, delta.x);

            float t = 1.0 - dist / RADIUS;
            float influence = t * t;
            float angularSpeed = mix(8.0, 30.0, influence);
            float nextAngle = angle + angularSpeed * dt;

            vec2 targetPos = mousePos + vec2(cos(nextAngle), sin(nextAngle)) * orbitRadius;
            vel = (targetPos - pos) / max(dt, 0.0001);
        }
    }

    vel *= 0.985;
    float maxVel = 1200.0;
    if (length(vel) > maxVel)
        vel = normalize(vel) * maxVel;

    vec2 prevPos = pos;
    pos += vel * dt;

    if (pos.x < 0.0 || pos.x >= screenRes.x) { vel.x *= -0.5; pos.x = clamp(pos.x, 0.0, screenRes.x - 1.0); }
    if (pos.y < 0.0 || pos.y >= screenRes.y) { vel.y *= -0.5; pos.y = clamp(pos.y, 0.0, screenRes.y - 1.0); }

    p.prevX = prevPos.x;
    p.prevY = prevPos.y;
    p.vx = vel.x;
    p.vy = vel.y;
    p.x = pos.x;
    p.y = pos.y;

    vec2 pDelta = pos - prevPos;
    float particleSpeed = length(pDelta);

    if (particleSpeed > 0.001)
    {
        float speedFactor = smoothstep(0.0, 4.0, particleSpeed);
        vec3 dyeColor = (particleSpeed < 1.0) ? vec3(0.0, 0.8, 1.0) : getVelocityColor(pDelta);

        float forceRadius = push.splatRadius;

        vec2 minPixel = min(prevPos, pos) - vec2(forceRadius);
        vec2 maxPixel = max(prevPos, pos) + vec2(forceRadius);

        ivec2 minGrid = clamp(ivec2(floor((minPixel / screenRes) * simRes)), ivec2(0), ivec2(int(push.simWidth) - 1, int(push.simHeight) - 1));
        ivec2 maxGrid = clamp(ivec2(ceil((maxPixel / screenRes) * simRes)),   ivec2(0), ivec2(int(push.simWidth) - 1, int(push.simHeight) - 1));

        float denomForce = max(forceRadius * forceRadius * 0.4, 0.0001);
        float denomColor = max(forceRadius * forceRadius * 0.243, 0.0001);
        float particleWeight = clamp(25000.0 / float(max(PARTICLE_COUNT, 1u)), 0.02, 1.0);

        for (int gy = minGrid.y; gy <= maxGrid.y; ++gy)
        {
            for (int gx = minGrid.x; gx <= maxGrid.x; ++gx)
            {
                vec2 cellPixelPos = ((vec2(gx, gy) + 0.5) / simRes) * screenRes;
                float dist = distToSegment(cellPixelPos, prevPos, pos);

                if (dist < forceRadius)
                {
                    ivec2 coord = ivec2(gx, gy);

                    float forceInf = exp(-(dist * dist) / denomForce);
                    vec2 pDeltaUV = pDelta / screenRes;
                    vec2 vFromParticle = pDeltaUV * push.splatForce * particleWeight;

                    vec2 currentV = imageLoad(inOutVelocity, coord).xy;
                    vec2 addedV = vFromParticle * forceInf * speedFactor;

                    vec2 newV = currentV + addedV;
                    float curSpeed = length(newV);
                    
                    const float MAX_FLUID_SPEED = 120.0;
                    if (curSpeed > MAX_FLUID_SPEED) {
                        newV = (newV / curSpeed) * (MAX_FLUID_SPEED + (curSpeed - MAX_FLUID_SPEED) * 0.1);
                    }

                    // Zapis prędkości bezpośrednio do tekstury 2D
                    imageStore(inOutVelocity, coord, vec4(newV, 0.0, 0.0));

                    float colorRadius = forceRadius * 0.9;
                    if (dist < colorRadius)
                    {
                        float colorInf = exp(-(dist * dist) / denomColor);
                        float mixIntensity = clamp(colorInf * 0.65 * (0.1 + 0.9 * speedFactor) * (particleWeight * 2.5), 0.0, 1.0);

                        vec4 currentC = imageLoad(inOutColor, coord);
                        vec3 newC = mix(currentC.rgb, dyeColor, mixIntensity);

                        // Zapis koloru bezpośrednio do tekstury 2D
                        imageStore(inOutColor, coord, vec4(newC, 1.0));
                    }
                }
            }
        }
    }

    particles[id] = p;
}