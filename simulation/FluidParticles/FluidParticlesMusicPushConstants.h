#pragma once

#include "../../audio/AudioAnalyzer.h"
#include "../../audio/AudioDeviationAnalyzer.h"
#include "../../audio/MusicPush.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

class FluidParticlesMusicPushConstants {
public:
    struct Data {
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
        // float audioCentroid;
        // float bpm;
        // float longBass;
        // float longTreble;
        // float longFlux;
        // float longOnset;
        // float longCentroid;
        // float shortTreble;
        // float shortFlux;
        // float shortOnset;
        // float deviation;
        // float deviationStrength;

        float dt;
        float splatRadius;
        float splatForce;
        float velocityDissipation;
        float densityDissipation;
        float vorticity;
        uint32_t simWidth;
        uint32_t simHeight;
        uint32_t windowWidth;
        uint32_t windowHeight;
    };

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
        uint32_t windowHeight)
    {
        data.rms = musicData.rms;
        data.bass = musicData.bass;
        data.mid = musicData.mid;
        data.treble = musicData.treble;
        data.flux = musicData.flux;
        data.onset = musicData.onset;
        data.bassEvent = musicData.bassEvent;
        data.impact = musicData.impact;
        data.beat = musicData.beat;
        data.longEnergy = musicData.longEnergy;
        data.longMid = musicData.longMid;
        data.shortEnergy = musicData.shortEnergy;
        data.shortBass = musicData.shortBass;
        data.shortMid = musicData.shortMid;
        data.shortCentroid = musicData.shortCentroid;
        data.bassDeviation = musicData.bassDeviation;
        data.direction = musicData.direction;

        data.dt = dt;
        data.splatRadius = splatRadius;
        data.splatForce = splatForce;
        data.velocityDissipation = velocityDissipation;
        data.densityDissipation = densityDissipation;
        data.vorticity = vorticity;
        data.simWidth = simWidth;
        data.simHeight = simHeight;
        data.windowWidth = windowWidth;
        data.windowHeight = windowHeight;
    }

    const Data &get() const noexcept {
        return data;
    }

    void reset() noexcept {
        data = {};
    }

private:
    Data data{};
};