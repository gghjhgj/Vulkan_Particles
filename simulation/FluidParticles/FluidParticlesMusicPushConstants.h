#pragma once

#include "../../audio/AudioAnalyzer.h"
#include "../../audio/AudioDeviationAnalyzer.h"
#include "../../audio/MusicPush.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

class FluidParticlesMusicPushConstants {
public:
    struct alignas(16) Data {
        float screenResX;
        float screenResY;
        float simResX;
        float simResY;

        float pannedCenterX;
        float pannedCenterY;
        float spawnOffsetX;
        float spawnOffsetY;

        float spawnPerpX;
        float spawnPerpY;
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
    };
    static_assert(sizeof(Data) == 128, "Data structure must be exactly 128 bytes!");

    void update(
        const MusicPush::Data &musicData,
        float dt,
        float splatRadius,
        float splatForce,
        float velocityDissipation,
        float densityDissipation,
        float vorticity,
        uint32_t simWidth,
        uint32_t simHeight,
        uint32_t windowWidth,
        uint32_t windowHeight,
        uint32_t particleCount = 100000)
    {
        float sWidth = (windowWidth > 0) ? static_cast<float>(windowWidth) : 1920.0f;
        float sHeight = (windowHeight > 0) ? static_cast<float>(windowHeight) : 1080.0f;
        float sSimW = (simWidth > 0) ? static_cast<float>(simWidth) : 320.0f;
        float sSimH = (simHeight > 0) ? static_cast<float>(simHeight) : 180.0f;

        data.screenResX = sWidth;
        data.screenResY = sHeight;
        data.simResX = sSimW;
        data.simResY = sSimH;

        float songPhase = calculateSongPhase(musicData);
        float baseRichHue = getRichMoodHue(songPhase, musicData.shortCentroid);
        data.baseRichHue = baseRichHue;

        float moodScale = mix(0.4f, 1.0f, songPhase);
        float dynamicVolume = 0.25f + (smoothstep(0.25f, 0.75f, musicData.longEnergy) * 0.35f) 
                                    + (smoothstep(0.35f, 0.75f, musicData.shortEnergy) * 0.5f);

        float centerX = sWidth * 0.5f;
        float centerY = sHeight * 0.5f;
        float panX = musicData.direction * 25.0f * moodScale;
        float panY = (musicData.shortCentroid - 0.5f) * 30.0f * moodScale;
        data.pannedCenterX = centerX + panX;
        data.pannedCenterY = centerY + panY;

        float peakTrigger = std::max(musicData.onset * 1.3f, musicData.beat);
        float bassSurge = musicData.bassEvent * (1.0f + musicData.bassDeviation * 1.4f);
        float burstEnergy = (std::max(musicData.impact, bassSurge) + musicData.flux * 0.6f) * peakTrigger;

        float pCount = static_cast<float>(std::max(particleCount, 1u));
        float ambientSpawnProb = 8.0f / pCount;
        float burstSpawnProb = mix(0.08f, 0.35f, songPhase);
        float burstThreshold = mix(0.85f, 1.55f, smoothstep(0.35f, 0.90f, musicData.longEnergy));
        float burstFactor = std::pow(smoothstep(burstThreshold, burstThreshold + 0.6f, burstEnergy), 3.0f) * (songPhase * songPhase);

        float rawDrop = std::max(musicData.impact * 0.75f, std::max(musicData.bassEvent, musicData.onset));
        float dropSignal = smoothstep(0.40f, 0.95f, rawDrop) * dynamicVolume * (0.2f + 0.8f * songPhase);
        data.baseTeleportProb = mix(ambientSpawnProb * dropSignal, burstSpawnProb, burstFactor);
        data.rms = musicData.rms;

        float spawnAngle = musicData.direction * 3.14159265f;
        float cosSpawn = std::cos(spawnAngle);
        float sinSpawn = std::sin(spawnAngle);
        float spawnMag = 10.0f + burstFactor * 120.0f;
        data.spawnOffsetX = cosSpawn * spawnMag;
        data.spawnOffsetY = sinSpawn * spawnMag;
        data.spawnPerpX = -sinSpawn;
        data.spawnPerpY = cosSpawn;

        float constantExpansion = dynamicVolume * 0.20f * moodScale;
        float vocalDynamics = std::max(musicData.shortMid - musicData.longMid * 0.65f, 0.0f) * 1.6f;
        float vocalPulse = (vocalDynamics + musicData.mid * 0.25f + musicData.shortEnergy * 0.15f) * dynamicVolume;
        float dynamicBassPunch = std::pow(musicData.bass * 1.25f + musicData.shortBass * 0.55f, 1.35f) * 1.55f;
        data.pulse = (constantExpansion + dynamicBassPunch * dynamicVolume + vocalPulse * 1.45f) * moodScale;

        float vocalColorShift = std::abs(musicData.shortCentroid - 0.5f) * 2.0f;
        float swirlBase = (musicData.mid * 1.15f + vocalColorShift * 1.6f + musicData.treble * 0.55f + musicData.flux * 0.75f) 
                          * dynamicVolume * moodScale * (1.0f + musicData.bassDeviation * 1.25f);
        data.swirl = swirlBase;
        data.swirlDir = swirlBase * (musicData.direction * 1.1f);

        data.driftScale = 0.28f * moodScale;
        data.driftPhase = musicData.shortEnergy * 4.0f;

        float baseSafeRadius = 215.0f;
        float bassBoost = std::pow(musicData.bass, 1.2f) * 160.0f;
        float onsetBoost = std::pow(musicData.onset, 1.1f) * 95.0f;
        float energySwelling = smoothstep(0.30f, 0.80f, musicData.shortEnergy) * 75.0f;
        data.safeRadius = baseSafeRadius + (bassBoost + onsetBoost + energySwelling + vocalDynamics * 50.0f) * moodScale;

        float isChorus = smoothstep(0.3f, 0.65f, musicData.longEnergy);
        data.centerAttract = mix(0.0016f, 0.0007f, isChorus);
        data.chorusPush = (dropSignal * 0.10f + burstFactor * 0.6f) * (2.2f + isChorus * 2.0f) * moodScale;

        float beatImpact = std::max(std::max(musicData.onset * 1.28f, musicData.bassEvent * 1.18f), musicData.impact * 1.08f);
        if (beatImpact > 0.15f) {
            data.popForce = (std::pow(beatImpact, 1.38f) * 15.2f + musicData.bassDeviation * 5.5f + musicData.flux * 3.8f) * moodScale;
            data.rimBurstThreshold = mix(0.972f, 0.930f, smoothstep(0.22f, 0.85f, beatImpact));
        } else {
            data.popForce = 0.0f;
            data.rimBurstThreshold = 1.0f;
        }

        data.velDamping = mix(0.955f, 0.92f, burstFactor);
        data.dynamicLimit = mix(20.0f, 36.0f, (burstFactor * moodScale) + dynamicVolume * 0.25f);
        data.dtFactor = dt * 60.0f;

        float activity = std::max(musicData.onset, std::max(musicData.flux, musicData.impact));
        data.hueLerpSpeed = mix(0.007f, 0.10f, activity);

        float kick = smoothstep(0.45f, 0.90f, musicData.bassEvent);
        float snare = smoothstep(0.40f, 0.90f, musicData.impact * musicData.mid);
        float rhythmOffset = (snare * 0.02f) - (kick * 0.06f);
        float freqShade = (musicData.treble * 0.03f) - (musicData.shortBass * 0.05f);
        data.baseHueOffset = rhythmOffset + freqShade;

        float kickFlash = smoothstep(0.55f, 0.95f, std::max(musicData.bassEvent, musicData.impact));
        data.kickFlashBright = kickFlash * 0.20f;
        float rawSat = mix(0.94f, 1.0f, std::clamp(baseRichHue + musicData.shortEnergy * 0.4f, 0.0f, 1.0f));
        data.saturation = std::clamp(rawSat - kickFlash * 0.12f, 0.78f, 1.0f);

        data.forceRadius = splatRadius * (1.0f + musicData.bass * 0.40f);
        data.splatForce = splatForce;
    }

    const Data &get() const noexcept {
        return data;
    }

    void reset() noexcept {
        data = {};
    }

private:
    Data data{};

    static inline float smoothstep(float edge0, float edge1, float x) noexcept {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    static inline float mix(float x, float y, float a) noexcept {
        return x * (1.0f - a) + y * a;
    }

    static inline float calculateSongPhase(const MusicPush::Data &m) noexcept {
        float rmsPower = smoothstep(0.06f, 0.22f, m.rms);
        float energySurge = std::max(m.shortEnergy - m.longEnergy * 0.75f, 0.0f) * 1.5f;
        float bassWeight = (m.shortBass * 0.65f + m.bass * 0.35f) * (1.0f + m.bassDeviation * 0.6f);
        float tension = (m.flux * 0.45f) + (m.treble * 0.35f) + std::max(m.shortMid - m.longMid * 0.7f, 0.0f) * 0.4f;
        float transientPower = std::max(m.impact * 0.9f, m.bassEvent * 1.1f);
        
        float phase = (m.longEnergy * 0.25f) + (rmsPower * 0.25f) + (m.shortEnergy * 0.20f) + (bassWeight * 0.30f);
        if (tension > 0.38f && bassWeight < 0.45f) {
            phase = mix(phase, 0.50f, smoothstep(0.35f, 0.75f, tension));
        }
        float dropEvidence = (transientPower * 0.45f) + (bassWeight * 0.35f) + (rmsPower * 0.20f) + energySurge * 0.3f;
        float isDrop = smoothstep(0.44f, 0.78f, dropEvidence);
        return std::clamp(mix(phase, 1.0f, isDrop * 0.85f), 0.0f, 1.0f);
    }

    static inline float getRichMoodHue(float songPhase, float audioCentroid) noexcept {
        float h;
        if (songPhase < 0.28f)       h = mix(0.85f, 0.70f, songPhase / 0.28f);
        else if (songPhase < 0.31f)  h = mix(0.70f, 0.50f, (songPhase - 0.28f) / 0.03f);
        else if (songPhase < 0.41f)  h = mix(0.50f, 0.33f, (songPhase - 0.31f) / 0.10f);
        else if (songPhase < 0.70f)  h = mix(0.33f, 0.10f, (songPhase - 0.41f) / 0.31f);
        else                         h = mix(0.10f, -0.06f, std::clamp((songPhase - 0.70f) / 0.22f, 0.0f, 1.0f));
        return h + (audioCentroid - 0.5f) * 0.10f;
    }
};