#pragma once

#include "AudioAnalyzer.h"
#include "AudioDeviationAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

class MusicPush {
public:
    struct Data {
        float rms;
        float bass;
        float mid;
        float treble;

        float centroid;
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
    };

    void update(const AudioAnalysis &analysis, const AudioDeviation &deviation, uint32_t sampleRate) {
        const float nyquist = static_cast<float>(sampleRate) * 0.5f;

        data.rms = clamp01(analysis.rms);
        data.bass = clamp01(analysis.bassLevel);
        data.mid = clamp01(analysis.midLevel);
        data.treble = clamp01(analysis.trebleLevel);

        data.centroid = normalizeFrequency(analysis.spectralCentroid, nyquist);
        data.flux = normalizePositive(analysis.spectralFlux, FLUX_NORMALIZATION);
        data.onset = normalizePositive(analysis.onsetStrength, ONSET_NORMALIZATION);

        data.bassEvent = clamp01(deviation.bassEventStrength);
        data.impact = clamp01(deviation.impact);
        data.beat = analysis.beat ? clamp01(analysis.beatScore) : 0.0f;
        data.bpm = normalizeBpm(analysis.bpm, BPM_MIN, BPM_MAX);

        data.longEnergy = encodeSigned(deviation.energy);
        data.longBass = encodeSigned(deviation.bass);
        data.longMid = encodeSigned(deviation.mid);
        data.longTreble = encodeSigned(deviation.treble);
        data.longFlux = encodeSigned(deviation.flux);
        data.longOnset = encodeSigned(deviation.onset);
        data.longCentroid = encodeSigned(deviation.centroid);

        data.shortEnergy = encodeSigned(deviation.shortEnergy);
        data.shortBass = encodeSigned(deviation.shortBass);
        data.shortMid = encodeSigned(deviation.shortMid);
        data.shortTreble = encodeSigned(deviation.shortTreble);
        data.shortFlux = encodeSigned(deviation.shortFlux);
        data.shortOnset = encodeSigned(deviation.shortOnset);
        data.shortCentroid = encodeSigned(deviation.shortCentroid);

        data.deviation = encodeSigned(deviation.deviation);
        data.deviationStrength = clamp01(deviation.strength);
        data.bassDeviation = encodeSigned(deviation.bassDeviation);
        data.direction = clamp11(deviation.direction);
    }

    const Data &get() const noexcept {
        return data;
    }

    void reset() noexcept {
        data = {};
    }

    static void printData(const Data &data) {
        std::cout
            << "rms: " << data.rms
            << " | bass: " << data.bass
            << " | mid: " << data.mid
            << " | treble: " << data.treble
            << " | centroid: " << data.centroid
            << " | flux: " << data.flux
            << " | onset: " << data.onset
            << " | bassEvent: " << data.bassEvent
            << " | impact: " << data.impact
            << " | beat: " << data.beat
            << " | bpm: " << data.bpm
            << " | longEnergy: " << data.longEnergy
            << " | longBass: " << data.longBass
            << " | longMid: " << data.longMid
            << " | longTreble: " << data.longTreble
            << " | longFlux: " << data.longFlux
            << " | longOnset: " << data.longOnset
            << " | longCentroid: " << data.longCentroid
            << " | shortEnergy: " << data.shortEnergy
            << " | shortBass: " << data.shortBass
            << " | shortMid: " << data.shortMid
            << " | shortTreble: " << data.shortTreble
            << " | shortFlux: " << data.shortFlux
            << " | shortOnset: " << data.shortOnset
            << " | shortCentroid: " << data.shortCentroid
            << " | deviation: " << data.deviation
            << " | deviationStrength: " << data.deviationStrength
            << " | bassDeviation: " << data.bassDeviation
            << " | direction: " << data.direction
            << '\n';
    }

private:
    static constexpr float FLUX_NORMALIZATION = 0.40f;
    static constexpr float ONSET_NORMALIZATION = 4.0f;
    static constexpr float BPM_MIN = 40.0f;
    static constexpr float BPM_MAX = 220.0f;

    static float clamp01(float value) {
        if (!std::isfinite(value))
            return 0.0f;
        return std::clamp(value, 0.0f, 1.0f);
    }

    static float clamp11(float value) {
        if (!std::isfinite(value))
            return 0.0f;
        return std::clamp(value, -1.0f, 1.0f);
    }

    static float normalizePositive(float value, float scale) {
        if (!std::isfinite(value) || scale <= 0.0f)
            return 0.0f;
        return clamp01(value / scale);
    }

    static float normalizeFrequency(float frequency, float maxFrequency) {
        if (!std::isfinite(frequency) || maxFrequency <= 0.0f)
            return 0.0f;
        return clamp01(frequency / maxFrequency);
    }

    static float encodeSigned(float value) {
        if (!std::isfinite(value))
            return 0.5f;
        value = clamp11(value);
        return clamp01(value * 0.5f + 0.5f);
    }

    static float normalizeBpm(float bpm, float minBpm, float maxBpm) {
        if (!std::isfinite(bpm) || maxBpm <= minBpm)
            return 0.0f;
        return clamp01((bpm - minBpm) / (maxBpm - minBpm));
    }

    Data data{};
};