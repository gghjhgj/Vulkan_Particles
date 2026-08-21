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

// Binding 0: Cząsteczki
layout(std430, set = 0, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

// Binding 1 i 2: Tekstury Płynu (Storage Images)
layout(rg32f, set = 0, binding = 1) uniform image2D inOutVelocity;
layout(rgba32f, set = 0, binding = 2) uniform image2D inOutColor;

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

float distToSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a;
    vec2 ba = b - a;
    float d = dot(ba, ba);
    float h = (d > 0.00001) ? clamp(dot(pa, ba) / d, 0.0, 1.0) : 0.0;
    return length(pa - ba * h);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float calculateSongPhase()
{
    float rmsPower = smoothstep(0.06, 0.22, push.rms);
    float energySurge = max(push.shortEnergy - push.longEnergy * 0.75, 0.0) * 1.5;
    float bassWeight = (push.shortBass * 0.65 + push.bass * 0.35) * (1.0 + push.bassDeviation * 0.6);
    float tension = (push.flux * 0.45) + (push.treble * 0.35) + max(push.shortMid - push.longMid * 0.7, 0.0) * 0.4;
    float transientPower = max(push.impact * 0.9, push.bassEvent * 1.1);
    
    float phase = (push.longEnergy * 0.25) + (rmsPower * 0.25) + (push.shortEnergy * 0.20) + (bassWeight * 0.30);
    
    if (tension > 0.38 && bassWeight < 0.45) {
        phase = mix(phase, 0.50, smoothstep(0.35, 0.75, tension));
    }
    
    float dropEvidence = (transientPower * 0.45) + (bassWeight * 0.35) + (rmsPower * 0.20) + energySurge * 0.3;
    float isDrop = smoothstep(0.44, 0.78, dropEvidence);
    phase = mix(phase, 1.0, isDrop * 0.85);
    
    return clamp(phase, 0.0, 1.0);
}

float getRichMoodHue(float songPhase, float audioCentroid) 
{
    float h;
    if (songPhase < 0.28) {
        h = mix(0.85, 0.70, songPhase / 0.28);
    } else if (songPhase < 0.31) {
        h = mix(0.70, 0.50, (songPhase - 0.28) / 0.03);
    } else if (songPhase < 0.41) {
        h = mix(0.50, 0.33, (songPhase - 0.31) / 0.10);
    } else if (songPhase < 0.70) {
        h = mix(0.33, 0.10, (songPhase - 0.41) / 0.31);
    } else {
        h = mix(0.10, -0.06, clamp((songPhase - 0.70) / 0.22, 0.0, 1.0));
    }
    
    h += (audioCentroid - 0.5) * 0.10;
    return h;
}

vec3 getVibrantAudioColor(inout float storedHue, uint id, float particleSpeed, float songPhase)
{
    uint strand = id % 4u;
    float strandShift = (strand == 0u) ? -0.06 : ((strand == 1u) ? 0.0 : ((strand == 2u) ? 0.05 : 0.09));

    float targetBaseHue = getRichMoodHue(songPhase, push.shortCentroid) + strandShift;
    float activity = max(push.onset, max(push.flux, push.impact));
    float lerpSpeed = mix(0.007, 0.10, activity); 
    
    float hueDiff = fract(targetBaseHue - storedHue + 0.5) - 0.5;
    storedHue = fract(storedHue + hueDiff * lerpSpeed);

    float kick = smoothstep(0.45, 0.90, push.bassEvent);
    float snare = smoothstep(0.40, 0.90, push.impact * push.mid); 
    
    float rhythmOffset = (snare * 0.02) - (kick * 0.06);
    float speedAccent = smoothstep(2.0, 20.0, particleSpeed) * 0.07;
    float freqShade = (push.treble * 0.03) - (push.shortBass * 0.05);

    float displayHue = fract(storedHue + rhythmOffset + freqShade - speedAccent);
    float brightness = mix(0.85, 1.0, smoothstep(0.0, 15.0, particleSpeed));
    
    float kickFlash = smoothstep(0.55, 0.95, max(push.bassEvent, push.impact));
    brightness = clamp(brightness + kickFlash * 0.20, 0.0, 1.0);
    
    float saturation = mix(0.94, 1.0, clamp(songPhase + push.shortEnergy * 0.4, 0.0, 1.0));
    saturation = clamp(saturation - kickFlash * 0.12, 0.78, 1.0);

    return hsv2rgb(vec3(displayHue, saturation, brightness));
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

    float songPhase = calculateSongPhase();

    if (isnan(p.x) || isnan(p.vx) || p.color == 0u)
    {
        p.x = screenRes.x * 0.5;
        p.y = screenRes.y * 0.5;
        p.vx = 0.0;
        p.vy = 0.0;
        
        uint strand = id % 4u;
        float strandShift = (strand == 0u) ? -0.06 : ((strand == 1u) ? 0.0 : ((strand == 2u) ? 0.05 : 0.09));
        float initHue = fract(getRichMoodHue(songPhase, push.shortCentroid) + strandShift);
        p.color = floatBitsToUint(initHue);
    }

    vec2 pos = vec2(p.x, p.y);
    vec2 vel = vec2(p.vx, p.vy);

    float moodScale = mix(0.4, 1.0, songPhase); 
    float dynamicVolume = 0.25 + (smoothstep(0.25, 0.75, push.longEnergy) * 0.35) + (smoothstep(0.35, 0.75, push.shortEnergy) * 0.5);

    vec2 center = screenRes * 0.5;
    float panX = push.direction * 25.0 * moodScale;
    float panY = (push.shortCentroid - 0.5) * 30.0 * moodScale;
    vec2 pannedCenter = center + vec2(panX, panY);
    
    vec2 fromCenter = pos - pannedCenter;
    float dist = length(fromCenter);

    float peakTrigger = max(push.onset * 1.3, push.beat);
    float bassSurge = push.bassEvent * (1.0 + push.bassDeviation * 1.4);
    float burstEnergy = (max(push.impact, bassSurge) + push.flux * 0.6) * peakTrigger;

    float randVal = random(float(id) * 12.9898 + push.rms * 78.233);
    float pCount = float(max(PARTICLE_COUNT, 1u));
    
    float ambientSpawnProb = 8.0 / pCount; 
    float burstSpawnProb = mix(0.08, 0.35, songPhase); 

    float burstThreshold = mix(0.85, 1.55, smoothstep(0.35, 0.90, push.longEnergy));
    float burstFactor = pow(smoothstep(burstThreshold, burstThreshold + 0.6, burstEnergy), 3.0) * (songPhase * songPhase);

    float rawDrop = max(push.impact * 0.75, max(push.bassEvent, push.onset));
    float dropSignal = smoothstep(0.40, 0.95, rawDrop) * dynamicVolume * (0.2 + 0.8 * songPhase);

    float baseTeleportProb = mix(ambientSpawnProb * dropSignal, burstSpawnProb, burstFactor);
    float distFactor = clamp(dist / (screenRes.y * 0.85), 0.0, 1.0);
    float finalProbability = baseTeleportProb * (0.04 + 1.6 * (distFactor * distFactor));

    if (randVal < finalProbability) 
    {
        float spawnAngle = push.direction * 3.14159265; 
        float scatterOffset = (fract(float(id) * 0.789) - 0.5) * 18.0;
        vec2 spawnOffset = vec2(cos(spawnAngle), sin(spawnAngle)) * (10.0 + burstFactor * 120.0);
        vec2 perpendicular = vec2(-sin(spawnAngle), cos(spawnAngle));
        
        pos = pannedCenter + spawnOffset + perpendicular * scatterOffset;
        vel = vec2(0.0);
        
        fromCenter = pos - pannedCenter;
        dist = length(fromCenter);

        uint strand = id % 4u;
        float strandShift = (strand == 0u) ? -0.06 : ((strand == 1u) ? 0.0 : ((strand == 2u) ? 0.05 : 0.09));
        float birthHue = fract(getRichMoodHue(songPhase, push.shortCentroid) + strandShift);
        p.color = floatBitsToUint(birthHue);
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
    float swirl = (push.mid * 1.15 + vocalColorShift * 1.6 + push.treble * 0.55 + push.flux * 0.75) 
                  * dynamicVolume * moodScale * (1.0 + push.bassDeviation * 1.25);

    float sideSign = (fract(float(id) * 0.543) > 0.5) ? 1.0 : -1.0;
    
    vel += dir * pulse;
    vel += tangent * swirl * (sideSign + push.direction * 1.1);

    float driftAngle = random(float(id) * 7.123) * 6.28318 + (push.shortEnergy * 4.0);
    vel += vec2(cos(driftAngle), sin(driftAngle)) * (0.28 * moodScale);

    float baseSafeRadius = 215.0;
    float bassBoost = pow(push.bass, 1.2) * 160.0;
    float onsetBoost = pow(push.onset, 1.1) * 95.0;
    float energySwelling = smoothstep(0.30, 0.80, push.shortEnergy) * 75.0;
    
    float safeRadius = baseSafeRadius + (bassBoost + onsetBoost + energySwelling + vocalDynamics * 50.0) * moodScale;

    float nearRim = smoothstep(safeRadius * 0.58, safeRadius * 1.05, dist);
    float rimBurstRand = fract(sin(float(id) * 91.345 + push.rms * 123.4) * 47453.1);
    float beatImpact = max(max(push.onset * 1.28, push.bassEvent * 1.18), push.impact * 1.08);
    float rimBurstThreshold = mix(0.972, 0.930, smoothstep(0.22, 0.85, beatImpact));

    if (nearRim > 0.10 && rimBurstRand > rimBurstThreshold && beatImpact > 0.15) 
    {
        float popForce = (pow(beatImpact, 1.38) * 15.2 + push.bassDeviation * 5.5 + push.flux * 3.8) * moodScale;
        vel += (dir * popForce) + (tangent * (fract(float(id) * 0.345) - 0.5) * 1.9 * popForce * 0.42);
    }

    float isChorus = smoothstep(0.3, 0.65, push.longEnergy);
    vel += dir * ((dropSignal * 0.10 + burstFactor * 0.6) * (2.2 + isChorus * 2.0) * moodScale);

    float radialSpeed = dot(vel, dir);
    vec2 radialVel = dir * radialSpeed;

    float innerRadiusFactor = smoothstep(60.0, 160.0, dist);
    vel -= fromCenter * (mix(0.0016, 0.0007, isChorus) * innerRadiusFactor);

    if (dist > safeRadius) 
    {
        float excess = dist - safeRadius;
        if (radialSpeed > 0.0) {
            vel -= radialVel * clamp(excess / 140.0, 0.0, 0.50);
        }
        vel -= dir * (excess * 0.022 * moodScale);
    }

    vel *= mix(0.955, 0.92, burstFactor); 

    float speed = length(vel);
    float dynamicLimit = mix(20.0, 36.0, (burstFactor * moodScale) + dynamicVolume * 0.25); 
    if (speed > dynamicLimit)
    {
        vel = normalize(vel) * dynamicLimit;
    }

    vec2 prevPos = pos;
    pos += vel * (push.dt * 60.0);

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

        float storedHue = uintBitsToFloat(p.color);
        vec3 dyeColor = getVibrantAudioColor(storedHue, id, particleSpeed, songPhase);
        p.color = floatBitsToUint(storedHue);

        float forceRadius = push.splatRadius * (1.0 + push.bass * 0.40);

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
                    ivec2 coord = ivec2(gx, gy);

                    float forceInf = exp(-(dSeg * dSeg) / denomForce);
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

                    imageStore(inOutVelocity, coord, vec4(newV, 0.0, 0.0));

                    float colorRadius = forceRadius * 0.9;
                    if (dSeg < colorRadius)
                    {
                        float colorInf = exp(-(dSeg * dSeg) / denomColor);
                        float mixIntensity = clamp(colorInf * 0.32 * (0.35 + 0.65 * speedFactor) * (particleWeight * 1.9), 0.0, 0.65);

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