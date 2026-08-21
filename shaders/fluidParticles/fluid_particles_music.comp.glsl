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

layout(std430, set = 0, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

layout(rg16f, set = 0, binding = 1) uniform image2D inOutVelocity;
layout(rgba8, set = 0, binding = 2) uniform image2D inOutColor;

layout(push_constant) uniform Push
{
    vec2 screenRes;            
    vec2 simRes;               

    vec2 pannedCenter;         
    vec2 spawnOffset;          

    vec2 spawnPerp;            
    float baseTeleportProb;    
    float rms;                 

    float pulse;               
    float swirl;               
    float swirlDir;            
    float safeRadius;          

    float driftScale; 
    float driftPhase; 
    float centerAttract;
    float chorusPush;

    float popForce;
    float rimBurstThreshold;   
    float velDamping;
    float dynamicLimit;

    float dtFactor;
    float baseRichHue;
    float hueLerpSpeed;
    float baseHueOffset;

    float kickFlashBright;
    float saturation;
    float forceRadius;
    float splatForce;
} push;

const float STRAND_SHIFTS[4] = float[4](-0.06, 0.0, 0.05, 0.09);

float random(float seed)
{
    return fract(sin(seed) * 43758.5453123);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= PARTICLE_COUNT) 
        return;

    Particle p = particles[id];

    vec2 invScreenRes = 1.0 / push.screenRes;
    vec2 simToScreen = push.screenRes / push.simRes;
    float strandShift = STRAND_SHIFTS[id & 3u];

    if (isnan(p.x) || isnan(p.vx) || p.color == 0u)
    {
        p.x = push.screenRes.x * 0.5;
        p.y = push.screenRes.y * 0.5;
        p.vx = 0.0;
        p.vy = 0.0;
        p.color = floatBitsToUint(fract(push.baseRichHue + strandShift));
    }

    vec2 pos = vec2(p.x, p.y);
    vec2 vel = vec2(p.vx, p.vy);

    vec2 fromCenter = pos - push.pannedCenter;
    float dist = length(fromCenter);

    float randVal = random(float(id) * 12.9898 + push.rms * 78.233);
    float distFactor = clamp(dist * (1.176470588 * invScreenRes.y), 0.0, 1.0);
    float finalProbability = push.baseTeleportProb * (0.04 + 1.6 * (distFactor * distFactor));

    if (randVal < finalProbability) 
    {
        float scatterOffset = (fract(float(id) * 0.789) - 0.5) * 18.0;
        pos = push.pannedCenter + push.spawnOffset + push.spawnPerp * scatterOffset;
        vel = vec2(0.0);
        
        fromCenter = pos - push.pannedCenter;
        dist = length(fromCenter);
        p.color = floatBitsToUint(fract(push.baseRichHue + strandShift));
    }

    vec2 dir = (dist > 0.0001) ? (fromCenter / dist) : vec2(cos(float(id) * 2.399963), sin(float(id) * 2.399963));
    vec2 tangent = vec2(-dir.y, dir.x);
    float sideSign = (fract(float(id) * 0.543) > 0.5) ? 1.0 : -1.0;

    vel += dir * push.pulse;
    vel += tangent * (push.swirl * sideSign + push.swirlDir);

    float driftAngle = random(float(id) * 7.123) * 6.2831853 + push.driftPhase;
    vel += vec2(cos(driftAngle), sin(driftAngle)) * push.driftScale;

    float nearRim = smoothstep(push.safeRadius * 0.58, push.safeRadius * 1.05, dist);
    if (nearRim > 0.10 && push.popForce > 0.0) 
    {
        float rimBurstRand = fract(sin(float(id) * 91.345 + push.rms * 123.4) * 47453.1);
        if (rimBurstRand > push.rimBurstThreshold)
        {
            vel += (dir * push.popForce) + (tangent * (fract(float(id) * 0.345) - 0.5) * 0.798 * push.popForce);
        }
    }

    vel += dir * push.chorusPush;

    float radialSpeed = dot(vel, dir);
    float innerRadiusFactor = smoothstep(60.0, 160.0, dist);
    vel -= fromCenter * (push.centerAttract * innerRadiusFactor);

    if (dist > push.safeRadius) 
    {
        float excess = dist - push.safeRadius;
        if (radialSpeed > 0.0) {
            vel -= dir * (radialSpeed * clamp(excess * 0.007142857, 0.0, 0.50));
        }
        vel -= dir * (excess * (push.driftScale * 0.07857143));
    }

    vel *= push.velDamping;

    float speedSq = dot(vel, vel);
    float maxSpeedSq = push.dynamicLimit * push.dynamicLimit;
    if (speedSq > maxSpeedSq)
    {
        vel *= (push.dynamicLimit / sqrt(speedSq));
    }

    vec2 prevPos = pos;
    pos += vel * push.dtFactor;

    if (pos.x < 0.0) { vel.x *= -0.5; pos.x = 0.0; }
    else if (pos.x >= push.screenRes.x) { vel.x *= -0.5; pos.x = push.screenRes.x - 1.0; }
    if (pos.y < 0.0) { vel.y *= -0.5; pos.y = 0.0; }
    else if (pos.y >= push.screenRes.y) { vel.y *= -0.5; pos.y = push.screenRes.y - 1.0; }

    p.prevX = prevPos.x;
    p.prevY = prevPos.y;
    p.vx = vel.x;
    p.vy = vel.y;
    p.x = pos.x;
    p.y = pos.y;

    vec2 pDelta = pos - prevPos;
    float pDeltaSq = dot(pDelta, pDelta);

    if (pDeltaSq > 0.000001)
    {
        float particleSpeed = sqrt(pDeltaSq);
        float invParticleSpeed = 1.0 / particleSpeed;
        float speedFactor = smoothstep(0.0, 4.0, particleSpeed);

        float storedHue = uintBitsToFloat(p.color);
        float targetBaseHue = push.baseRichHue + strandShift;
        float hueDiff = fract(targetBaseHue - storedHue + 0.5) - 0.5;
        storedHue = fract(storedHue + hueDiff * push.hueLerpSpeed);

        float speedAccent = smoothstep(2.0, 20.0, particleSpeed) * 0.07;
        float displayHue = fract(storedHue + push.baseHueOffset - speedAccent);
        float brightness = clamp(mix(0.85, 1.0, smoothstep(0.0, 15.0, particleSpeed)) + push.kickFlashBright, 0.0, 1.0);

        vec3 dyeColor = hsv2rgb(vec3(displayHue, push.saturation, brightness));
        p.color = floatBitsToUint(storedHue);

        float forceRadius = push.forceRadius;
        float forceRadiusSq = forceRadius * forceRadius;
        float colorRadiusSq = forceRadiusSq * 0.81;
        float invForceRadius = 1.0 / max(forceRadius, 0.001);

        vec2 minPixel = min(prevPos, pos) - vec2(forceRadius);
        vec2 maxPixel = max(prevPos, pos) + vec2(forceRadius);

        ivec2 gridMaxBound = ivec2(push.simRes) - ivec2(1);
        ivec2 minGrid = clamp(ivec2(floor((minPixel * invScreenRes) * push.simRes)), ivec2(0), gridMaxBound);
        ivec2 maxGrid = clamp(ivec2(ceil((maxPixel * invScreenRes) * push.simRes)),   ivec2(0), gridMaxBound);

        float invDenomForce = 1.0 / max(forceRadiusSq * 0.4, 0.0001);
        float invDenomColor = 1.0 / max(forceRadiusSq * 0.243, 0.0001);

        vec2 fwd = pDelta * invParticleSpeed;
        vec2 side = vec2(-fwd.y, fwd.x);

        vec2 pDeltaUV = pDelta * invScreenRes;
        float splatBaseMag = length(pDeltaUV) * push.splatForce * speedFactor;
        float invPDeltaSq = 1.0 / pDeltaSq;
        float colorIntensityFactor = 0.32 * (0.35 + 0.65 * speedFactor);

        for (int gy = minGrid.y; gy <= maxGrid.y; ++gy)
        {
            float cellY = (float(gy) + 0.5) * simToScreen.y;
            for (int gx = minGrid.x; gx <= maxGrid.x; ++gx)
            {
                vec2 cellPixelPos = vec2((float(gx) + 0.5) * simToScreen.x, cellY);
                vec2 toCell = cellPixelPos - prevPos;

                float h = clamp(dot(toCell, pDelta) * invPDeltaSq, 0.0, 1.0);
                vec2 dVec = toCell - pDelta * h;
                float dSegSq = dot(dVec, dVec);

                if (dSegSq < forceRadiusSq)
                {
                    ivec2 coord = ivec2(gx, gy);

                    float forceInf = exp(-dSegSq * invDenomForce);
                    float sideDist = dot(toCell, side);
                    float swirlVal = clamp(sideDist * invForceRadius, -1.0, 1.0);
                    vec2 forceDir = fwd * 0.7 + side * (swirlVal * 1.3);

                    vec2 addedV = forceDir * (splatBaseMag * forceInf);
                    vec2 currentV = imageLoad(inOutVelocity, coord).xy;
                    vec2 newV = currentV + addedV;
                    
                    float curSpeedSq = dot(newV, newV);
                    const float MAX_FLUID_SPEED = 400.0;
                    const float MAX_FLUID_SPEED_SQ = 160000.0;

                    if (curSpeedSq > MAX_FLUID_SPEED_SQ) {
                        float curSpeed = sqrt(curSpeedSq);
                        newV = (newV / curSpeed) * (MAX_FLUID_SPEED + (curSpeed - MAX_FLUID_SPEED) * 0.1);
                    }

                    imageStore(inOutVelocity, coord, vec4(newV, 0.0, 0.0));

                    if (dSegSq < colorRadiusSq)
                    {
                        float colorInf = exp(-dSegSq * invDenomColor);
                        float mixIntensity = clamp(colorInf * colorIntensityFactor, 0.0, 0.65);

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