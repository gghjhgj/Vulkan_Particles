#include "AudioDeviationAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
    constexpr float MAD_TO_STD = 1.4826f;
    constexpr float IMPACT_ONSET_WEIGHT = 0.45f;
    constexpr float IMPACT_FLUX_WEIGHT = 0.30f;
    constexpr float IMPACT_ENERGY_WEIGHT = 0.25f;
    constexpr float SHORT_WEIGHT = 0.70f;
    constexpr float LONG_WEIGHT = 0.30f;
    constexpr float SHORT_BASS_WEIGHT = 0.75f;
    constexpr float LONG_BASS_WEIGHT = 0.25f;
}

AudioDeviationAnalyzer::AudioDeviationAnalyzer() {
    m_longHistoryMs = AudioConfig::audio.deviation_long_history_ms;
    m_shortHistoryMs = AudioConfig::audio.deviation_short_history_ms;
    m_energyScaleDb = AudioConfig::audio.energyScaleDb;
    m_normalizedScale = AudioConfig::audio.normalizedScale;
    m_fluxScale = AudioConfig::audio.fluxScale;
    m_onsetScale = AudioConfig::audio.onsetScale;
    m_centroidScaleHz = AudioConfig::audio.centroidScaleHz;

    if (m_longHistoryMs <= 0.0f) m_longHistoryMs = 2000.0f;
    if (m_shortHistoryMs <= 0.0f) m_shortHistoryMs = 150.0f;
    if (m_shortHistoryMs > m_longHistoryMs) m_shortHistoryMs = m_longHistoryMs;
    if (m_energyScaleDb <= 0.0f) m_energyScaleDb = 2.0f;
    if (m_normalizedScale <= 0.0f) m_normalizedScale = 0.025f;
    if (m_fluxScale <= 0.0f) m_fluxScale = 0.04f;
    if (m_onsetScale <= 0.0f) m_onsetScale = 0.75f;
    if (m_centroidScaleHz <= 0.0f) m_centroidScaleHz = 1200.0f;
}

AudioDeviation AudioDeviationAnalyzer::analyze(const AudioAnalysis &analysis, float deltaMs) {
    AudioDeviation result;

    if (deltaMs <= 0.0f)
        deltaMs = 10.0f;

    m_maxLongHistorySamples = std::max(size_t(5), static_cast<size_t>(std::ceil(m_longHistoryMs / deltaMs)));
    m_maxShortHistorySamples = std::max(size_t(5), static_cast<size_t>(std::ceil(m_shortHistoryMs / deltaMs)));

    auto calculate = [](float current, const AudioDeviationAnalyzer::History &history, float scale) {
        if (history.values.size() < 5)
            return 0.0f;

        const float med = AudioDeviationAnalyzer::median(history.values);
        const float madValue = AudioDeviationAnalyzer::mad(history.values, med);
        return AudioDeviationAnalyzer::signedDeviation(current, med, madValue, scale);
    };

    const float energy = sanitize(analysis.energyDb);
    const float bass = sanitize(analysis.bassLevel);
    const float mid = sanitize(analysis.midLevel);
    const float treble = sanitize(analysis.trebleLevel);
    const float flux = sanitize(analysis.spectralFlux);
    const float onset = sanitize(analysis.onsetStrength);
    const float centroid = sanitize(analysis.spectralCentroid);

    result.energy = calculate(energy, m_energyHistory, m_energyScaleDb);
    result.bass = calculate(bass, m_bassHistory, m_normalizedScale);
    result.mid = calculate(mid, m_midHistory, m_normalizedScale);
    result.treble = calculate(treble, m_trebleHistory, m_normalizedScale);
    result.flux = calculate(flux, m_fluxHistory, m_fluxScale);
    result.onset = calculate(onset, m_onsetHistory, m_onsetScale);
    result.centroid = calculate(centroid, m_centroidHistory, m_centroidScaleHz);

    result.shortEnergy = calculate(energy, m_shortEnergyHistory, m_energyScaleDb);
    result.shortBass = calculate(bass, m_shortBassHistory, m_normalizedScale);
    result.shortMid = calculate(mid, m_shortMidHistory, m_normalizedScale);
    result.shortTreble = calculate(treble, m_shortTrebleHistory, m_normalizedScale);
    result.shortFlux = calculate(flux, m_shortFluxHistory, m_fluxScale);
    result.shortOnset = calculate(onset, m_shortOnsetHistory, m_onsetScale);
    result.shortCentroid = calculate(centroid, m_shortCentroidHistory, m_centroidScaleHz);

    result.energyStrength = strengthFromDeviation(result.energy);
    result.bassStrength = strengthFromDeviation(result.bass);
    result.midStrength = strengthFromDeviation(result.mid);
    result.trebleStrength = strengthFromDeviation(result.treble);
    result.fluxStrength = strengthFromDeviation(result.flux);
    result.onsetStrength = strengthFromDeviation(result.onset);
    result.centroidStrength = strengthFromDeviation(result.centroid);

    result.shortEnergyStrength = strengthFromDeviation(result.shortEnergy);
    result.shortBassStrength = strengthFromDeviation(result.shortBass);
    result.shortMidStrength = strengthFromDeviation(result.shortMid);
    result.shortTrebleStrength = strengthFromDeviation(result.shortTreble);
    result.shortFluxStrength = strengthFromDeviation(result.shortFlux);
    result.shortOnsetStrength = strengthFromDeviation(result.shortOnset);
    result.shortCentroidStrength = strengthFromDeviation(result.shortCentroid);

    const float combinedEnergy = result.shortEnergy * SHORT_WEIGHT + result.energy * LONG_WEIGHT;
    const float combinedBass = result.shortBass * SHORT_WEIGHT + result.bass * LONG_WEIGHT;
    const float combinedMid = result.shortMid * SHORT_WEIGHT + result.mid * LONG_WEIGHT;
    const float combinedTreble = result.shortTreble * SHORT_WEIGHT + result.treble * LONG_WEIGHT;
    const float combinedFlux = result.shortFlux * SHORT_WEIGHT + result.flux * LONG_WEIGHT;
    const float combinedOnset = result.shortOnset * SHORT_WEIGHT + result.onset * LONG_WEIGHT;
    const float combinedCentroid = result.shortCentroid * 0.60f + result.centroid * 0.40f;

    const float weighted = combinedEnergy * 0.30f +
                           combinedBass * 0.25f +
                           combinedMid * 0.15f +
                           combinedTreble * 0.10f +
                           combinedFlux * 0.10f +
                           combinedOnset * 0.10f;

    result.deviation = clamp11(weighted);
    result.strength = clamp01(std::fabs(result.deviation));

    result.bassDeviation = clamp11(result.shortBass * SHORT_BASS_WEIGHT + result.bass * LONG_BASS_WEIGHT);
    result.bassEventStrength = clamp01(std::fabs(result.bassDeviation));

    const float shortImpact = clamp01(result.shortOnsetStrength * IMPACT_ONSET_WEIGHT +
                                      result.shortFluxStrength * IMPACT_FLUX_WEIGHT +
                                      result.shortEnergyStrength * IMPACT_ENERGY_WEIGHT);

    const float longImpact = clamp01(result.onsetStrength * IMPACT_ONSET_WEIGHT +
                                     result.fluxStrength * IMPACT_FLUX_WEIGHT +
                                     result.energyStrength * IMPACT_ENERGY_WEIGHT);

    result.impact = clamp01(shortImpact * 0.75f + longImpact * 0.25f);

    if (analysis.beat) {
        result.impact = clamp01(result.impact + analysis.beatScore * 0.25f);
    }

    constexpr float DIRECTION_SCALE = 0.12f;

    const float energyDirection = result.shortEnergy - m_previousEnergy;
    const float bassDirection = result.shortBass - m_previousBass;
    const float midDirection = result.shortMid - m_previousMid;
    const float trebleDirection = result.shortTreble - m_previousTreble;

    const float rawDirection = energyDirection * 0.40f +
                               bassDirection * 0.25f +
                               midDirection * 0.15f +
                               trebleDirection * 0.20f;

    if (!m_hasPreviousDirection) {
        result.direction = 0.0f;
        m_hasPreviousDirection = true;
    } else {
        result.direction = clamp11(std::tanh(rawDirection / DIRECTION_SCALE));
    }

    m_previousEnergy = result.shortEnergy;
    m_previousBass = result.shortBass;
    m_previousMid = result.shortMid;
    m_previousTreble = result.shortTreble;

    const float strengths[] = {
        clamp01(std::fabs(combinedEnergy)),
        clamp01(std::fabs(combinedBass)),
        clamp01(std::fabs(combinedMid)),
        clamp01(std::fabs(combinedTreble)),
        clamp01(std::fabs(combinedFlux)),
        clamp01(std::fabs(combinedOnset)),
        clamp01(std::fabs(combinedCentroid))
    };

    float maximum = 0.0f;
    for (int i = 0; i < 7; ++i) {
        if (strengths[i] > maximum) {
            maximum = strengths[i];
            result.dominantFeature = i;
        }
    }

    push(m_energyHistory, energy, m_maxLongHistorySamples);
    push(m_bassHistory, bass, m_maxLongHistorySamples);
    push(m_midHistory, mid, m_maxLongHistorySamples);
    push(m_trebleHistory, treble, m_maxLongHistorySamples);
    push(m_fluxHistory, flux, m_maxLongHistorySamples);
    push(m_onsetHistory, onset, m_maxLongHistorySamples);
    push(m_centroidHistory, centroid, m_maxLongHistorySamples);

    push(m_shortEnergyHistory, energy, m_maxShortHistorySamples);
    push(m_shortBassHistory, bass, m_maxShortHistorySamples);
    push(m_shortMidHistory, mid, m_maxShortHistorySamples);
    push(m_shortTrebleHistory, treble, m_maxShortHistorySamples);
    push(m_shortFluxHistory, flux, m_maxShortHistorySamples);
    push(m_shortOnsetHistory, onset, m_maxShortHistorySamples);
    push(m_shortCentroidHistory, centroid, m_maxShortHistorySamples);

    result.historySize = m_energyHistory.values.size();
    result.shortHistorySize = m_shortEnergyHistory.values.size();

    return result;
}

void AudioDeviationAnalyzer::reset() {
    m_energyHistory.values.clear();
    m_bassHistory.values.clear();
    m_midHistory.values.clear();
    m_trebleHistory.values.clear();
    m_fluxHistory.values.clear();
    m_onsetHistory.values.clear();
    m_centroidHistory.values.clear();

    m_shortEnergyHistory.values.clear();
    m_shortBassHistory.values.clear();
    m_shortMidHistory.values.clear();
    m_shortTrebleHistory.values.clear();
    m_shortFluxHistory.values.clear();
    m_shortOnsetHistory.values.clear();
    m_shortCentroidHistory.values.clear();

    m_previousEnergy = 0.0f;
    m_previousBass = 0.0f;
    m_previousMid = 0.0f;
    m_previousTreble = 0.0f;

    m_hasPreviousDirection = false;
}

float AudioDeviationAnalyzer::median(const std::deque<float> &values) {
    if (values.empty())
        return 0.0f;

    std::vector<float> sorted(values.begin(), values.end());
    std::sort(sorted.begin(), sorted.end());

    const size_t n = sorted.size();
    if (n & 1) {
        return sorted[n / 2];
    }

    return (sorted[n / 2 - 1] + sorted[n / 2]) * 0.5f;
}

float AudioDeviationAnalyzer::mad(const std::deque<float> &values, float medianValue) {
    if (values.empty())
        return 0.0f;

    std::vector<float> deviations;
    deviations.reserve(values.size());

    for (const float value : values) {
        deviations.push_back(std::fabs(value - medianValue));
    }

    std::sort(deviations.begin(), deviations.end());

    const size_t n = deviations.size();
    if (n & 1) {
        return deviations[n / 2];
    }

    return (deviations[n / 2 - 1] + deviations[n / 2]) * 0.5f;
}

float AudioDeviationAnalyzer::clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float AudioDeviationAnalyzer::clamp11(float value) {
    return std::clamp(value, -1.0f, 1.0f);
}

float AudioDeviationAnalyzer::signedDeviation(float current, float medianValue, float madValue, float minimumScale) {
    const float robustScale = std::max(madValue * MAD_TO_STD, minimumScale);
    const float z = (current - medianValue) / robustScale;
    return clamp11(std::tanh(z * 0.65f));
}

float AudioDeviationAnalyzer::strengthFromDeviation(float deviation) {
    return clamp01(std::fabs(deviation));
}

void AudioDeviationAnalyzer::push(History &history, float value, size_t maxSamples) {
    history.values.push_back(value);
    while (history.values.size() > maxSamples) {
        history.values.pop_front();
    }
}

float AudioDeviationAnalyzer::sanitize(float value) {
    if (!std::isfinite(value))
        return 0.0f;
    return value;
}