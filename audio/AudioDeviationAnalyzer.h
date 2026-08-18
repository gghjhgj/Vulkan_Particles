#pragma once

#include "AudioAnalyzer.h"
#include "AudioConfig/AudioConfig.h"

#include <cstddef>
#include <deque>

struct AudioDeviation {
    float energy = 0.0f;
    float bass = 0.0f;
    float mid = 0.0f;
    float treble = 0.0f;
    float flux = 0.0f;
    float onset = 0.0f;
    float centroid = 0.0f;

    float energyStrength = 0.0f;
    float bassStrength = 0.0f;
    float midStrength = 0.0f;
    float trebleStrength = 0.0f;
    float fluxStrength = 0.0f;
    float onsetStrength = 0.0f;
    float centroidStrength = 0.0f;

    float shortEnergy = 0.0f;
    float shortBass = 0.0f;
    float shortMid = 0.0f;
    float shortTreble = 0.0f;
    float shortFlux = 0.0f;
    float shortOnset = 0.0f;
    float shortCentroid = 0.0f;

    float shortEnergyStrength = 0.0f;
    float shortBassStrength = 0.0f;
    float shortMidStrength = 0.0f;
    float shortTrebleStrength = 0.0f;
    float shortFluxStrength = 0.0f;
    float shortOnsetStrength = 0.0f;
    float shortCentroidStrength = 0.0f;

    float deviation = 0.0f;
    float strength = 0.0f;

    float bassDeviation = 0.0f;
    float bassEventStrength = 0.0f;

    float impact = 0.0f;
    float direction = 0.0f;

    int dominantFeature = -1;

    size_t historySize = 0;
    size_t shortHistorySize = 0;
};

class AudioDeviationAnalyzer {
public:
    AudioDeviationAnalyzer();

    AudioDeviation analyze(const AudioAnalysis &analysis, float deltaMs);
    void reset();

private:
    struct History {
        std::deque<float> values;
    };

    float m_longHistoryMs = 2000.0f;
    float m_shortHistoryMs = 150.0f;

    size_t m_maxLongHistorySamples = 1;
    size_t m_maxShortHistorySamples = 1;

    float m_energyScaleDb = 2.0f;
    float m_normalizedScale = 0.025f;
    float m_fluxScale = 0.04f;
    float m_onsetScale = 0.75f;
    float m_centroidScaleHz = 1200.0f;

    History m_energyHistory;
    History m_bassHistory;
    History m_midHistory;
    History m_trebleHistory;
    History m_fluxHistory;
    History m_onsetHistory;
    History m_centroidHistory;

    History m_shortEnergyHistory;
    History m_shortBassHistory;
    History m_shortMidHistory;
    History m_shortTrebleHistory;
    History m_shortFluxHistory;
    History m_shortOnsetHistory;
    History m_shortCentroidHistory;

    bool m_hasPreviousDirection = false;
    float m_previousEnergy = 0.0f;
    float m_previousBass = 0.0f;
    float m_previousMid = 0.0f;
    float m_previousTreble = 0.0f;

    static float median(const std::deque<float> &values);
    static float mad(const std::deque<float> &values, float medianValue);
    static float clamp01(float value);
    static float clamp11(float value);
    static float signedDeviation(float current, float medianValue, float madValue, float minimumScale);
    static float strengthFromDeviation(float deviation);
    static void push(History &history, float value, size_t maxSamples);
    static float sanitize(float value);
};