#version 450
#extension GL_GOOGLE_include_directive : require

#include "../common/particle.glsl"
#include "../common/fluid_push.glsl"
#include "../common/math.glsl"
#include "../common/color.glsl"

layout(local_size_x = 256) in;

layout(constant_id = 0) const uint WORKGROUP_SIZE = 256;
layout(constant_id = 1) const uint PARTICLE_COUNT = 0;

layout(std430, set = 0, binding = 0) buffer ParticleBuffer
{
    Particle particles[];
};

layout(rg16f, set = 0, binding = 1) uniform image2D inOutVelocity;
layout(rgba8, set = 0, binding = 2) uniform image2D inOutColor;

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

vec3 getEnergyPaletteColor(float speed, float scale)
{
    float s = clamp(speed / (9.0 * scale), 0.0, 2.0);

    vec3 c0 = vec3(0.05, 0.45, 0.95);
    vec3 c1 = vec3(0.55, 0.15, 0.95);
    vec3 c2 = vec3(0.98, 0.40, 0.05);
    vec3 c3 = vec3(1.00, 0.95, 0.85);

    if (s < 0.5)
        return mix(c0, c1, s * 2.0);
    else if (s < 1.0)
        return mix(c1, c2, (s - 0.5) * 2.0);
    else
        return mix(c2, c3, clamp(s - 1.0, 0.0, 1.0));
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

    float scale = screenRes.y / 1080.0;

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

    float gridToPixel = (screenRes.x / simRes.x) * 15.0;
    float pixelToGrid = 1.0 / max(gridToPixel, 0.0001);

    vec2 uv = pos / screenRes;
    vec2 fluidVelGrid = sampleFluidVelocity(uv);
    vec2 fluidVelPixels = fluidVelGrid * gridToPixel;

    float fluidDragCoeff = 3.2;
    float fluidDragFactor = 1.0 - exp(-fluidDragCoeff * dt);
    vel += (fluidVelPixels - vel) * fluidDragFactor;

    bool isRightDown = (push.isMouseDown & 2) != 0;
    if (isRightDown)
    {
        vec2 mousePos = vec2(push.mouseX, push.mouseY);
        vec2 delta = pos - mousePos;
        float dist = length(delta);

        float maxRadius  = 950.0 * scale;
        float coreRadius = 40.0 * scale;

        if (dist > 1.0 && dist < maxRadius)
        {
            float normDist = dist / maxRadius;
            float t = 1.0 - normDist;
            float influence = smoothstep(0.0, 1.0, t);

            vec2 tangent = vec2(-delta.y, delta.x) / dist;
            vec2 radial  = -delta / dist;

            float orbitSpeed = mix(200.0 * scale, 1400.0 * scale, pow(t, 0.6));
            float pullCurve  = sin(normDist * 3.14159265);
            float pullSpeed  = mix(80.0 * scale, 600.0 * scale, pullCurve) * influence;

            if (dist < coreRadius)
            {
                pullSpeed *= (dist / coreRadius);
            }

            vec2 targetVel = (tangent * orbitSpeed) + (radial * pullSpeed);
            float driveRate = mix(3.5, 12.0, influence);
            vel += (targetVel - vel) * (1.0 - exp(-driveRate * dt));
        }
    }

    vel *= pow(0.995, dt * 60.0);
    
    float maxVel = 4000.0 * scale;
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
    float moveDistPixels = length(pDelta);

    if (moveDistPixels > 0.001)
    {
        float speedFactor = smoothstep(0.0, 3.5 * scale, moveDistPixels);
        vec3 dyeColor = getEnergyPaletteColor(moveDistPixels, scale);

        vec2 particleGridVel = vel * pixelToGrid;

        float splatRadiusPixels = max(push.splatRadius * scale, 1.0);

        vec2 cellSizePixels = screenRes / simRes;
        float minCellDim = min(cellSizePixels.x, cellSizePixels.y);

        float stepSizePixels = max(splatRadiusPixels * 0.75, minCellDim * 0.5);
        int numSteps = clamp(int(ceil(moveDistPixels / stepSizePixels)), 1, 24);
        float stepWeight = 1.0 / float(numSteps);

        float denomPixels = max(splatRadiusPixels * splatRadiusPixels * 0.45, 0.001);
        float transferFractionBase = (1.0 - exp(-fluidDragCoeff * dt * 2.5)) * speedFactor;

        int maxW = int(push.simWidth) - 1;
        int maxH = int(push.simHeight) - 1;

        for (int step = 0; step < numSteps; ++step)
        {
            float t = (float(step) + 0.5) / float(numSteps);
            vec2 samplePosPixels = mix(prevPos, pos, t);

            vec2 minPixel = samplePosPixels - vec2(splatRadiusPixels);
            vec2 maxPixel = samplePosPixels + vec2(splatRadiusPixels);

            ivec2 minCell = clamp(ivec2(floor((minPixel / screenRes) * simRes)), ivec2(0), ivec2(maxW, maxH));
            ivec2 maxCell = clamp(ivec2(ceil((maxPixel / screenRes) * simRes)),  ivec2(0), ivec2(maxW, maxH));

            for (int gy = minCell.y; gy <= maxCell.y; ++gy)
            {
                for (int gx = minCell.x; gx <= maxCell.x; ++gx)
                {
                    ivec2 coord = ivec2(gx, gy);
                    
                    vec2 cellPixelCenter = ((vec2(coord) + 0.5) / simRes) * screenRes;
                    float distPixels = length(cellPixelCenter - samplePosPixels);

                    if (distPixels <= splatRadiusPixels)
                    {
                        float fade = 1.0 - (distPixels / splatRadiusPixels);
                        float w = exp(-(distPixels * distPixels) / denomPixels) * fade * stepWeight;

                        vec2 currentFluidV = imageLoad(inOutVelocity, coord).xy;
                        vec2 relVel = particleGridVel - currentFluidV;
                        
                        float transferRate = clamp(w * transferFractionBase * 0.4, 0.0, 0.45);
                        vec2 newFluidV = currentFluidV + relVel * transferRate;
                        imageStore(inOutVelocity, coord, vec4(newFluidV, 0.0, 0.0));

                        float mixIntensity = clamp(w * 0.5 * (0.1 + 0.9 * speedFactor), 0.0, 0.85);
                        vec4 currentC = imageLoad(inOutColor, coord);
                        vec3 newC = mix(currentC.rgb, dyeColor, mixIntensity);
                        imageStore(inOutColor, coord, vec4(newC, 1.0));
                    }
                }
            }
        }
    }

    particles[id] = p;
}