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

struct FluidCell
{
    float vx;
    float vy;
    float pressure;
    float divergence;
    float r;
    float g;
    float b;
    float a;
};

layout(std430, set = 0, binding = 0) buffer ParticleBuffer
{
    Particle particles[];
};

layout(std430, set = 0, binding = 1) buffer FluidBuffer
{
    FluidCell fluidCells[];
};

layout(push_constant) uniform Push
{
    float rms;
    float bass;
    float mid;
    float treble;
    float flux;
    float onset;
    float bassEvent;
    float impact;
    float beat;
    float longEnergy;
    float longMid;
    float shortEnergy;
    float shortBass;
    float shortMid;
    float shortCentroid;
    float bassDeviation;
    float direction;

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
} push;

float random(float seed)
{
    return fract(sin(seed) * 43758.5453123);
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

    vec2 simRes = vec2(
        push.simWidth > 0 ? float(push.simWidth) : 320.0,
        push.simHeight > 0 ? float(push.simHeight) : 180.0
    );

    if (isnan(p.x) || isnan(p.vx))
    {
        p.x = screenRes.x * 0.5;
        p.y = screenRes.y * 0.5;
        p.vx = 0.0;
        p.vy = 0.0;
    }

    vec2 pos = vec2(p.x, p.y);
    vec2 vel = vec2(p.vx, p.vy);

    float isChorus = smoothstep(0.3, 0.65, push.longEnergy);
    float trackMood = smoothstep(0.25, 0.65, push.longEnergy);
    float moodScale = mix(0.4, 1.0, trackMood); 

    float dynamicVolume = 0.25 + (smoothstep(0.25, 0.75, push.longEnergy) * 0.35) + (smoothstep(0.35, 0.75, push.shortEnergy) * 0.5);

    vec2 center = screenRes * 0.5;
    float panX = push.direction * 25.0 * moodScale;
    float panY = (push.shortCentroid - 0.5) * 30.0 * moodScale;
    vec2 pannedCenter = center + vec2(panX, panY);
    
    vec2 fromCenter = pos - pannedCenter;
    float dist = length(fromCenter);

    float peakTrigger = max(push.onset * 1.3, push.beat);
    float bassSurge = push.bassEvent * (1.0 + push.bassDeviation * 1.4);
    float energyContext = max(push.impact, bassSurge) + push.flux * 0.6;
    float armageddonPower = energyContext * peakTrigger;

    float randVal = random(float(id) * 12.9898 + push.rms * 78.233);
    float pCount = float(max(PARTICLE_COUNT, 1u));
    
    float probForSmallSpit = 8.0 / pCount; 
    float probForArmageddon = mix(0.08, 0.35, trackMood); 

    float baseWall = mix(0.85, 1.55, smoothstep(0.35, 0.90, push.longEnergy));
    float armageddonFactor = smoothstep(baseWall, baseWall + 0.6, armageddonPower);
    armageddonFactor = pow(armageddonFactor, 3.0) * (trackMood * trackMood);

    float rawDrop = max(push.impact * 0.75, max(push.bassEvent, push.onset));
    float dropSignal = smoothstep(0.40, 0.95, rawDrop) * dynamicVolume * (0.2 + 0.8 * trackMood);

    float baseTeleportProb = mix(probForSmallSpit * dropSignal, probForArmageddon, armageddonFactor);
    
    float distFactor = clamp(dist / (screenRes.y * 0.85), 0.0, 1.0);
    float finalProbability = baseTeleportProb * (0.04 + 1.6 * (distFactor * distFactor));

    if (randVal < finalProbability) 
    {
        float spawnAngle = push.direction * 3.14159265; 
        float scatterOffset = (fract(float(id) * 0.789) - 0.5) * 18.0;
        vec2 spawnOffset = vec2(cos(spawnAngle), sin(spawnAngle)) * (10.0 + armageddonFactor * 120.0);
        vec2 perpendicular = vec2(-sin(spawnAngle), cos(spawnAngle));
        
        pos = pannedCenter + spawnOffset + perpendicular * scatterOffset;
        vel = vec2(0.0);
        
        fromCenter = pos - pannedCenter;
        dist = length(fromCenter);
    }

    float angle = float(id) * 2.399963;
    vec2 dir = (dist > 0.0001) ? normalize(fromCenter) : vec2(cos(angle), sin(angle));
    vec2 tangent = vec2(-dir.y, dir.x);

    float constantExpansion = dynamicVolume * 0.20 * moodScale;
    
    float vocalDynamics = max(push.shortMid - push.longMid * 0.65, 0.0) * 1.6;
    float vocalPulse = (vocalDynamics + push.mid * 0.25 + push.shortEnergy * 0.15) * dynamicVolume;
    
    float dynamicBassPunch = pow(push.bass * 1.25 + push.shortBass * 0.55, 1.35) * 1.55;
    float pulse = (constantExpansion + dynamicBassPunch * dynamicVolume + vocalPulse * 1.45) * moodScale;
    
    float vocalColorShift = abs(push.shortCentroid - 0.5) * 2.0;
    float bassDeviationFactor = push.bassDeviation * 1.25;
    
    float swirl = (push.mid * 1.15 + vocalColorShift * 1.6 + push.treble * 0.55 + push.flux * 0.75) 
                  * dynamicVolume * moodScale * (1.0 + bassDeviationFactor);

    float sideSign = (fract(float(id) * 0.543) > 0.5) ? 1.0 : -1.0;
    float swirlBias = push.direction * 1.1;
    
    vel += dir * pulse;
    vel += tangent * swirl * (sideSign + swirlBias);

    float driftAngle = random(float(id) * 7.123) * 6.28318 + (push.shortEnergy * 4.0);
    vec2 fluidDrift = vec2(cos(driftAngle), sin(driftAngle)) * (0.28 * moodScale);
    vel += fluidDrift;

    float baseSafeRadius = 215.0;
    float bassBoost = pow(push.bass, 1.2) * 160.0;
    float onsetBoost = pow(push.onset, 1.1) * 95.0;
    float energySwelling = smoothstep(0.30, 0.80, push.shortEnergy) * 75.0;
    
    float dynamicExpansion = (bassBoost + onsetBoost + energySwelling + vocalDynamics * 50.0) * moodScale;
    float safeRadius = baseSafeRadius + dynamicExpansion;

    float nearRim = smoothstep(safeRadius * 0.65, safeRadius * 1.06, dist);
    float pchelkaRand = fract(sin(float(id) * 91.345 + push.rms * 123.4) * 47453.1);

    float beatImpact = max(max(push.onset * 1.25, push.bassEvent * 1.15), push.impact);
    float pchelkaThreshold = mix(0.985, 0.955, smoothstep(0.3, 0.9, beatImpact));

    if (nearRim > 0.15 && pchelkaRand > pchelkaThreshold && beatImpact > 0.20) 
    {
        float popForce = (pow(beatImpact, 1.45) * 13.5 + push.bassDeviation * 5.0 + push.flux * 3.5) * moodScale;
        float tangentScatter = (fract(float(id) * 0.345) - 0.5) * 1.8;
        vel += (dir * popForce) + (tangent * tangentScatter * popForce * 0.40);
    }

    float explosionPower = (dropSignal * 0.10) + (armageddonFactor * 0.6);
    float explosion = explosionPower * (2.2 + isChorus * 2.0) * moodScale;
    vel += dir * explosion;

    float radialSpeed = dot(vel, dir);
    vec2 radialVel = dir * radialSpeed;

    float fluidPull = mix(0.0016, 0.0007, smoothstep(0.3, 0.65, push.longEnergy));
    float innerRadiusFactor = smoothstep(60.0, 160.0, dist);
    vel -= fromCenter * (fluidPull * innerRadiusFactor);

    if (dist > safeRadius) 
    {
        float excess = dist - safeRadius;
        
        if (radialSpeed > 0.0) {
            vel -= radialVel * clamp(excess / 140.0, 0.0, 0.50);
        }
        vel -= dir * (excess * 0.022 * moodScale);
    }

    float friction = mix(0.955, 0.92, armageddonFactor);
    vel *= friction; 

    float speed = length(vel);
    
    float dynamicLimit = mix(20.0, 36.0, (armageddonFactor * moodScale) + dynamicVolume * 0.25); 
    if (speed > dynamicLimit)
    {
        vel = normalize(vel) * dynamicLimit;
    }

    vec2 prevPos = pos;
    pos += vel;

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

        float forceRadius = push.splatRadius * (1.0 + push.bass * 0.4);

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
                
                float dSeg = distToSegment(cellPixelPos, prevPos, pos);

                if (dSeg < forceRadius)
                {
                    int cellIdx = gy * int(push.simWidth) + gx;

                    float forceInf = exp(-(dSeg * dSeg) / denomForce);
                    
                    vec2 pDeltaUV = pDelta / screenRes;
                    vec2 vFromParticle = pDeltaUV * push.splatForce * particleWeight;

                    vec2 currentV = vec2(fluidCells[cellIdx].vx, fluidCells[cellIdx].vy);
                    vec2 addedV = vFromParticle * forceInf * speedFactor;

                    vec2 newV = currentV + addedV;
                    float curSpeed = length(newV);
                    
                    const float MAX_FLUID_SPEED = 120.0;
                    if (curSpeed > MAX_FLUID_SPEED) {
                        newV = (newV / curSpeed) * (MAX_FLUID_SPEED + (curSpeed - MAX_FLUID_SPEED) * 0.1);
                    }

                    fluidCells[cellIdx].vx = newV.x;
                    fluidCells[cellIdx].vy = newV.y;

                    float colorRadius = forceRadius * 0.9;
                    if (dSeg < colorRadius)
                    {
                        float colorInf = exp(-(dSeg * dSeg) / denomColor);
                        float mixIntensity = clamp(colorInf * 0.65 * (0.1 + 0.9 * speedFactor) * (particleWeight * 2.5), 0.0, 1.0);

                        fluidCells[cellIdx].r = mix(fluidCells[cellIdx].r, dyeColor.r, mixIntensity);
                        fluidCells[cellIdx].g = mix(fluidCells[cellIdx].g, dyeColor.g, mixIntensity);
                        fluidCells[cellIdx].b = mix(fluidCells[cellIdx].b, dyeColor.b, mixIntensity);
                        fluidCells[cellIdx].a = 1.0;
                    }
                }
            }
        }
    }

    particles[id] = p;
}