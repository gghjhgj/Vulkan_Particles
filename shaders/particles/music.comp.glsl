#version 450
#include "common/particle.glsl"

layout(constant_id = 1) const uint PARTICLE_COUNT = 0;
layout(local_size_x = 256) in;

layout(set = 0, binding = 0) buffer Particles
{
    Particle particles[];
};

layout(push_constant) uniform Push
{
    float rms;
    float bass;
    float mid;
    float treble;
    float audioCentroid;
    float flux;
    float onset;
    float bassEvent;
    float impact;
    float beat;
    float bpm;
    float longEnergy;
    float longBass;
    float longMid;
    float longTreble;
    float longFlux;
    float longOnset;
    float longCentroid;
    float shortEnergy;
    float shortBass;
    float shortMid;
    float shortTreble;
    float shortFlux;
    float shortOnset;
    float shortCentroid;
    float deviation;
    float deviationStrength;
    float bassDeviation;
    float direction;
} push;

float random(float seed) {
    return fract(sin(seed) * 43758.5453123);
}

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= PARTICLE_COUNT) return;

    Particle p = particles[id];

    vec2 pos = vec2(p.x, p.y);
    vec2 vel = vec2(p.vx, p.vy);

    float isChorus = smoothstep(0.3, 0.65, push.longEnergy);

    float trackMood = smoothstep(0.25, 0.65, push.longEnergy);
    float moodScale = mix(0.4, 1.0, trackMood); 

    float dynamicVolume = 0.25 + (smoothstep(0.25, 0.75, push.longEnergy) * 0.35) + (smoothstep(0.35, 0.75, push.shortEnergy) * 0.5);

    vec2 center = vec2(960.0, 540.0);
    float panX = push.direction * 25.0 * moodScale;
    float panY = (push.shortCentroid - 0.5) * 30.0 * moodScale;
    vec2 pannedCenter = center + vec2(panX, panY);
    
    vec2 fromCenter = pos - pannedCenter;
    float dist = length(fromCenter);

    float peakTrigger = max(push.onset * 1.3, push.beat);
    float bassSurge = push.bassEvent * (1.0 + push.bassDeviation * 1.4);
    float energyContext = max(push.impact, bassSurge) + push.flux * 0.6;
    float burstEnergy = energyContext * peakTrigger;

    float randVal = random(float(id) * 12.9898 + push.rms * 78.233);
    float pCount = float(max(PARTICLE_COUNT, 1));
    
    float ambientSpawnProb = 8.0 / pCount; 
    float burstSpawnProb = mix(0.08, 0.35, trackMood); 

    float burstThreshold = mix(0.85, 1.55, smoothstep(0.35, 0.90, push.longEnergy));
    float burstFactor = smoothstep(burstThreshold, burstThreshold + 0.6, burstEnergy);
    burstFactor = pow(burstFactor, 3.0) * (trackMood * trackMood);

    float rawDrop = max(push.impact * 0.75, max(push.bassEvent, push.onset));
    float dropSignal = smoothstep(0.40, 0.95, rawDrop) * dynamicVolume * (0.2 + 0.8 * trackMood);

    float baseTeleportProb = mix(ambientSpawnProb * dropSignal, burstSpawnProb, burstFactor);
    
    float distFactor = clamp(dist / 900.0, 0.0, 1.0);
    float finalProbability = baseTeleportProb * (0.04 + 1.6 * (distFactor * distFactor));

    if (randVal < finalProbability) 
    {
        float spawnAngle = push.direction * 3.14159; 
        float scatterOffset = (fract(float(id) * 0.789) - 0.5) * 18.0;
        vec2 spawnOffset = vec2(cos(spawnAngle), sin(spawnAngle)) * (10.0 + burstFactor * 120.0);
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
    float rimBurstRand = fract(sin(float(id) * 91.345 + push.rms * 123.4) * 47453.1);

    float beatImpact = max(max(push.onset * 1.25, push.bassEvent * 1.15), push.impact);
    float rimBurstThreshold = mix(0.985, 0.955, smoothstep(0.3, 0.9, beatImpact));

    if (nearRim > 0.15 && rimBurstRand > rimBurstThreshold && beatImpact > 0.20) 
    {
        float popForce = (pow(beatImpact, 1.45) * 13.5 + push.bassDeviation * 5.0 + push.flux * 3.5) * moodScale;
        float tangentScatter = (fract(float(id) * 0.345) - 0.5) * 1.8;
        vel += (dir * popForce) + (tangent * tangentScatter * popForce * 0.40);
    }

    float explosionPower = (dropSignal * 0.10) + (burstFactor * 0.6);
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

    float friction = mix(0.955, 0.92, burstFactor);
    vel *= friction; 

    float speed = length(vel);
    
    float dynamicLimit = mix(20.0, 36.0, (burstFactor * moodScale) + dynamicVolume * 0.25); 
    if (speed > dynamicLimit)
    {
        vel = normalize(vel) * dynamicLimit;
    }

    p.prevX = p.x;
    p.prevY = p.y;

    p.vx = vel.x;
    p.vy = vel.y;

    p.x = pos.x + vel.x; 
    p.y = pos.y + vel.y;

    particles[id] = p;
}