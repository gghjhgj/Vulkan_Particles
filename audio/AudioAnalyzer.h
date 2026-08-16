#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

class AudioBuffer;

struct FrequencyBand
{
    float linear = 0.0f;
    float db = -80.0f;
    float normalized = 0.0f;
};

struct FrequencyBands
{
    FrequencyBand subBass;
    FrequencyBand bass;
    FrequencyBand lowMid;
    FrequencyBand mid;
    FrequencyBand highMid;
    FrequencyBand treble;
    FrequencyBand high;
};

struct AudioAnalysis
{
    float rms = 0.0f;
    float rmsDb = -80.0f;

    float peak = 0.0f;
    float meanAbs = 0.0f;
    float dcOffset = 0.0f;
    float crestFactor = 0.0f;
    float zeroCrossingRate = 0.0f;

    std::vector<float> spectrum;
    std::vector<float> spectrumDb;

    float frequencyResolution = 0.0f;

    float spectralCentroid = 0.0f;
    float spectralRolloff = 0.0f;
    float spectralFlatness = 0.0f;
    float spectralFlux = 0.0f;

    FrequencyBands bands;

    float bassLevel = 0.0f;
    float midLevel = 0.0f;
    float trebleLevel = 0.0f;

    float energy = 0.0f;
    float energyDb = -80.0f;

    float energyDelta = 0.0f;

    float attack = 0.0f;
    float release = 0.0f;

    float globalOnsetStrength = 0.0f;
    float bassOnsetStrength = 0.0f;
    float onsetStrength = 0.0f;

    bool beat = false;
    float beatScore = 0.0f;

    float bpm = 0.0f;
    float beatInterval = 0.0f;

    uint64_t analysisFrame = 0;
};

class AudioAnalyzer
{
public:
    AudioAnalyzer(
        uint32_t fftSize,
        uint32_t sampleRate,
        float smoothingAttackMs,
        float smoothingReleaseMs,
        float spectrumFloorDb,
        float spectrumCeilingDb,
        float spectralRolloffPercent,
        float beatSensitivity,
        uint32_t beatCooldownMs,
        float bpmMin,
        float bpmMax
    );

    AudioAnalysis analyze(
        const AudioBuffer& buffer,
        size_t newFrames
    );

private:
    uint32_t m_fftSize;
    uint32_t m_sampleRate;

    float m_smoothingAttackMs;
    float m_smoothingReleaseMs;

    float m_spectrumFloorDb;
    float m_spectrumCeilingDb;

    float m_spectralRolloffPercent;

    float m_beatSensitivity;
    uint32_t m_beatCooldownMs;

    float m_bpmMin;
    float m_bpmMax;

    uint64_t m_analysisFrame = 0;

    double m_timeSeconds = 0.0;

    float m_previousSmoothedEnergy = 0.0f;
    float m_smoothedEnergy = 0.0f;

    float m_previousBassLevel = 0.0f;

    std::vector<float> m_previousSpectrum;

    std::deque<float> m_fluxHistory;
    std::deque<float> m_bassOnsetHistory;
    std::deque<float> m_onsetHistory;

    std::deque<double> m_beatTimes;
    std::deque<float> m_tempoHistory;

    float m_currentBpm = 0.0f;

    static constexpr size_t FLUX_HISTORY_SIZE = 150;
    static constexpr size_t BASS_ONSET_HISTORY_SIZE = 150;
    static constexpr size_t ONSET_PEAK_HISTORY_SIZE = 5;
    static constexpr size_t BPM_HISTORY_SIZE = 8;

    static constexpr size_t MIN_HISTORY_SIZE = 20;

    static constexpr float TEMPO_TOLERANCE = 0.07f;
    static constexpr float PHASE_TOLERANCE = 0.20f;

    static constexpr int MAX_BEAT_MULTIPLE = 4;

    void calculateTimeDomain(
        const std::vector<float>& mono,
        AudioAnalysis& result
    ) const;

    void calculateSpectrum(
        const std::vector<float>& mono,
        AudioAnalysis& result
    );

    void calculateSpectralFeatures(
        AudioAnalysis& result
    ) const;

    void calculateBands(
        AudioAnalysis& result
    ) const;

    void updateDynamics(
        AudioAnalysis& result,
        size_t newFrames
    );

    void updateRhythm(
        AudioAnalysis& result
    );

    void updateTempo(
        float beatInterval,
        AudioAnalysis& result
    );

    bool isLocalOnsetPeak(
        float onset
    ) const;

    bool isBeatTimingValid(
        double currentTime
    ) const;

    float smooth(
        float current,
        float previous,
        float deltaMs
    ) const;

    float normalizeDb(
        float db
    ) const;

    static float safeDb(
        float value
    );

    static float safePowerDb(
        float value
    );

    static float clamp01(
        float value
    );

    static float average(
        const std::deque<float>& values
    );

    static float standardDeviation(
        const std::deque<float>& values,
        float mean
    );

    static float median(
        const std::deque<float>& values
    );
};