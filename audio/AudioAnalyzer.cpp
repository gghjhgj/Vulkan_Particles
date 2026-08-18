#include "AudioAnalyzer.h"
#include "AudioBuffer.h"
#include "FFT.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
    constexpr float PI = 3.14159265358979323846f;
    constexpr float EPSILON = 1.0e-12f;
    constexpr float SILENCE_RMS = 1.0e-6f;
    constexpr float ONSET_ENERGY_FLOOR_DB = -48.0f;
    constexpr float ONSET_ENERGY_FULL_DB = -20.0f;

    float hannWindow(size_t index, size_t size) {
        if (size <= 1)
            return 1.0f;

        return 0.5f * (1.0f - std::cos(2.0f * PI * static_cast<float>(index) / static_cast<float>(size - 1)));
    }

    struct BandRange {
        float minHz;
        float maxHz;
        FrequencyBand* band;
    };
}

AudioAnalyzer::AudioAnalyzer(
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
)
    : m_fftSize(fftSize),
      m_sampleRate(sampleRate),
      m_smoothingAttackMs(smoothingAttackMs),
      m_smoothingReleaseMs(smoothingReleaseMs),
      m_spectrumFloorDb(spectrumFloorDb),
      m_spectrumCeilingDb(spectrumCeilingDb),
      m_spectralRolloffPercent(spectralRolloffPercent),
      m_beatSensitivity(beatSensitivity),
      m_beatCooldownMs(beatCooldownMs),
      m_bpmMin(bpmMin),
      m_bpmMax(bpmMax)
{
    if (m_fftSize < 2) {
        throw std::invalid_argument("AudioAnalyzer: FFT size too small");
    }

    if ((m_fftSize & (m_fftSize - 1)) != 0) {
        throw std::invalid_argument("AudioAnalyzer: FFT size must be power of two");
    }

    if (m_sampleRate == 0) {
        throw std::invalid_argument("AudioAnalyzer: sample rate cannot be zero");
    }

    if (m_smoothingAttackMs <= 0.0f || m_smoothingReleaseMs <= 0.0f) {
        throw std::invalid_argument("AudioAnalyzer: smoothing values must be positive");
    }

    if (m_spectrumFloorDb >= m_spectrumCeilingDb) {
        throw std::invalid_argument("AudioAnalyzer: invalid spectrum dB range");
    }

    if (m_spectralRolloffPercent <= 0.0f || m_spectralRolloffPercent >= 1.0f) {
        throw std::invalid_argument("AudioAnalyzer: rolloff must be between 0 and 1");
    }

    if (m_bpmMin <= 0.0f || m_bpmMax <= m_bpmMin) {
        throw std::invalid_argument("AudioAnalyzer: invalid BPM range");
    }
}

AudioAnalysis AudioAnalyzer::analyze(const AudioBuffer& buffer, size_t newFrames) {
    AudioAnalysis result;
    result.analysisFrame = ++m_analysisFrame;

    const uint32_t channels = buffer.channels();
    const uint32_t sampleRate = buffer.sampleRate();

    if (channels == 0 || sampleRate == 0) {
        return result;
    }

    m_timeSeconds += static_cast<double>(newFrames) / static_cast<double>(sampleRate);

    const size_t requiredSamples = static_cast<size_t>(m_fftSize) * channels;
    std::vector<float> interleaved(requiredSamples);

    const size_t received = buffer.readLatest(interleaved.data(), requiredSamples);
    const size_t availableFrames = received / channels;

    if (availableFrames < m_fftSize)
        return result;

    std::vector<float> mono(availableFrames);

    for (size_t frame = 0; frame < availableFrames; ++frame) {
        float sum = 0.0f;
        for (uint32_t channel = 0; channel < channels; ++channel) {
            sum += interleaved[frame * channels + channel];
        }
        mono[frame] = sum / static_cast<float>(channels);
    }

    calculateTimeDomain(mono, result);

    if (result.rms < SILENCE_RMS) {
        result.rms = 0.0f;
        result.rmsDb = -100.0f;
        result.peak = 0.0f;
        result.meanAbs = 0.0f;
        result.dcOffset = 0.0f;
        result.crestFactor = 0.0f;
        result.zeroCrossingRate = 0.0f;

        result.energy = 0.0f;
        result.energyDb = -100.0f;
        result.energyDelta = 0.0f;
        result.attack = 0.0f;
        result.release = 0.0f;

        result.spectralCentroid = 0.0f;
        result.spectralRolloff = 0.0f;
        result.spectralFlatness = 0.0f;
        result.spectralFlux = 0.0f;

        result.bassLevel = 0.0f;
        result.midLevel = 0.0f;
        result.trebleLevel = 0.0f;

        result.globalOnsetStrength = 0.0f;
        result.bassOnsetStrength = 0.0f;
        result.onsetStrength = 0.0f;

        result.beat = false;
        result.beatScore = 0.0f;
        result.beatInterval = 0.0f;

        result.bpm = 0.0f;

        m_previousSpectrum.clear();
        m_previousBassLevel = 0.0f;

        m_fluxHistory.clear();
        m_bassOnsetHistory.clear();
        m_onsetHistory.clear();

        m_beatTimes.clear();
        m_tempoHistory.clear();

        m_currentBpm = 0.0f;
        m_smoothedEnergy = 0.0f;
        m_previousSmoothedEnergy = 0.0f;

        return result;
    }

    std::vector<float> fftInput(mono.end() - m_fftSize, mono.end());

    calculateSpectrum(fftInput, result);
    calculateSpectralFeatures(result);
    calculateBands(result);

    result.energy = result.rms * result.rms;
    result.rmsDb = safeDb(result.rms);

    result.bassLevel = (result.bands.subBass.normalized + result.bands.bass.normalized) * 0.5f;
    result.midLevel = (result.bands.lowMid.normalized + result.bands.mid.normalized + result.bands.highMid.normalized) / 3.0f;
    result.trebleLevel = (result.bands.treble.normalized + result.bands.high.normalized) * 0.5f;

    updateDynamics(result, newFrames);
    updateRhythm(result);

    return result;
}

void AudioAnalyzer::calculateTimeDomain(const std::vector<float>& mono, AudioAnalysis& result) const {
    if (mono.empty())
        return;

    float sum = 0.0f;
    float sumSquares = 0.0f;
    float meanAbs = 0.0f;
    float peak = 0.0f;
    size_t zeroCrossings = 0;

    float previous = mono.front();

    for (const float sample : mono) {
        sum += sample;
        sumSquares += sample * sample;

        const float magnitude = std::fabs(sample);
        meanAbs += magnitude;
        peak = std::max(peak, magnitude);

        if ((previous < 0.0f && sample >= 0.0f) || (previous >= 0.0f && sample < 0.0f)) {
            ++zeroCrossings;
        }

        previous = sample;
    }

    const float count = static_cast<float>(mono.size());
    result.dcOffset = sum / count;
    result.meanAbs = meanAbs / count;
    result.rms = std::sqrt(sumSquares / count);
    result.peak = peak;
    result.crestFactor = result.peak / std::max(result.rms, EPSILON);
    result.rmsDb = safeDb(result.rms);

    if (mono.size() > 1) {
        result.zeroCrossingRate = static_cast<float>(zeroCrossings) / static_cast<float>(mono.size() - 1);
    }
}

void AudioAnalyzer::calculateSpectrum(const std::vector<float>& mono, AudioAnalysis& result) {
    const size_t n = mono.size();
    std::vector<float> windowed(n);

    for (size_t i = 0; i < n; ++i) {
        windowed[i] = mono[i] * hannWindow(i, n);
    }

    std::vector<std::complex<float>> fft(n);
    FFT::forward(windowed.data(), fft);

    const size_t bins = n / 2 + 1;
    result.spectrum.resize(bins);
    result.spectrumDb.resize(bins);

    const float normalization = 2.0f / static_cast<float>(n);

    for (size_t i = 0; i < bins; ++i) {
        float magnitude = std::abs(fft[i]) * normalization;

        if (i == 0 || i == n / 2) {
            magnitude *= 0.5f;
        }

        result.spectrum[i] = magnitude;
        result.spectrumDb[i] = std::max(m_spectrumFloorDb, safeDb(magnitude));
    }

    result.frequencyResolution = static_cast<float>(m_sampleRate) / static_cast<float>(m_fftSize);
}

void AudioAnalyzer::calculateSpectralFeatures(AudioAnalysis& result) const {
    if (result.spectrum.empty())
        return;

    const float binWidth = result.frequencyResolution;
    double magnitudeSum = 0.0;
    double weightedFrequency = 0.0;

    for (size_t i = 0; i < result.spectrum.size(); ++i) {
        const float magnitude = result.spectrum[i];
        const float frequency = static_cast<float>(i) * binWidth;

        magnitudeSum += magnitude;
        weightedFrequency += static_cast<double>(frequency) * magnitude;
    }

    if (magnitudeSum > EPSILON) {
        result.spectralCentroid = static_cast<float>(weightedFrequency / magnitudeSum);
    }

    double totalPower = 0.0;
    for (const float magnitude : result.spectrum) {
        totalPower += static_cast<double>(magnitude) * static_cast<double>(magnitude);
    }

    if (totalPower > EPSILON) {
        const double target = totalPower * static_cast<double>(m_spectralRolloffPercent);
        double accumulated = 0.0;

        for (size_t i = 0; i < result.spectrum.size(); ++i) {
            const double magnitude = result.spectrum[i];
            accumulated += magnitude * magnitude;

            if (accumulated >= target) {
                result.spectralRolloff = static_cast<float>(i) * binWidth;
                break;
            }
        }
    }

    double logSum = 0.0;
    double arithmeticSum = 0.0;
    const size_t count = result.spectrum.size();

    for (const float magnitude : result.spectrum) {
        const double power = static_cast<double>(magnitude) * static_cast<double>(magnitude) + EPSILON;
        logSum += std::log(power);
        arithmeticSum += power;
    }

    if (count > 0 && arithmeticSum > EPSILON) {
        const double geometric = std::exp(logSum / static_cast<double>(count));
        const double arithmetic = arithmeticSum / static_cast<double>(count);
        result.spectralFlatness = clamp01(static_cast<float>(geometric / arithmetic));
    }
}

void AudioAnalyzer::calculateBands(AudioAnalysis& result) const {
    if (result.spectrum.empty())
        return;

    BandRange ranges[] = {
        {20.0f, 60.0f, &result.bands.subBass},
        {60.0f, 250.0f, &result.bands.bass},
        {250.0f, 500.0f, &result.bands.lowMid},
        {500.0f, 2000.0f, &result.bands.mid},
        {2000.0f, 4000.0f, &result.bands.highMid},
        {4000.0f, 10000.0f, &result.bands.treble},
        {10000.0f, 20000.0f, &result.bands.high}
    };

    const float binWidth = result.frequencyResolution;

    for (auto& range : ranges) {
        const size_t firstBin = std::max(size_t(1), static_cast<size_t>(std::ceil(range.minHz / binWidth)));
        const size_t lastBin = std::min(result.spectrum.size() - 1, static_cast<size_t>(std::floor(range.maxHz / binWidth)));

        if (firstBin > lastBin)
            continue;

        float sum = 0.0f;
        size_t count = 0;

        for (size_t bin = firstBin; bin <= lastBin; ++bin) {
            sum += result.spectrum[bin];
            ++count;
        }

        if (count == 0)
            continue;

        const float averageMagnitude = sum / static_cast<float>(count);
        range.band->linear = averageMagnitude;
        range.band->db = safeDb(averageMagnitude);
        range.band->normalized = normalizeDb(range.band->db);
    }
}

void AudioAnalyzer::updateDynamics(AudioAnalysis& result, size_t newFrames) {
    const float deltaMs = 1000.0f * static_cast<float>(newFrames) / static_cast<float>(m_sampleRate);

    m_smoothedEnergy = smooth(result.energy, m_smoothedEnergy, deltaMs);
    result.energyDelta = m_smoothedEnergy - m_previousSmoothedEnergy;

    if (result.energyDelta > 0.0f) {
        result.attack = result.energyDelta;
        result.release = 0.0f;
    } else {
        result.attack = 0.0f;
        result.release = -result.energyDelta;
    }

    result.energy = m_smoothedEnergy;
    result.energyDb = safePowerDb(m_smoothedEnergy);
    m_previousSmoothedEnergy = m_smoothedEnergy;
}

void AudioAnalyzer::updateRhythm(AudioAnalysis& result) {
    float flux = 0.0f;

    if (!m_previousSpectrum.empty() && m_previousSpectrum.size() == result.spectrum.size()) {
        float currentSum = 0.0f;
        float previousSum = 0.0f;

        for (const float value : result.spectrum) {
            currentSum += value;
        }
        for (const float value : m_previousSpectrum) {
            previousSum += value;
        }

        currentSum = std::max(currentSum, EPSILON);
        previousSum = std::max(previousSum, EPSILON);

        for (size_t i = 0; i < result.spectrum.size(); ++i) {
            const float current = result.spectrum[i] / currentSum;
            const float previous = m_previousSpectrum[i] / previousSum;
            const float difference = current - previous;

            if (difference > 0.0f) {
                flux += difference;
            }
        }
    }

    result.spectralFlux = flux;

    const float currentBassLevel = (result.bands.subBass.linear + result.bands.bass.linear) * 0.5f;
    float bassRiseDb = 0.0f;

    if (m_previousBassLevel > EPSILON && currentBassLevel > EPSILON) {
        bassRiseDb = std::max(0.0f, safeDb(currentBassLevel) - safeDb(m_previousBassLevel));
    }

    const bool enoughHistory = m_fluxHistory.size() >= MIN_HISTORY_SIZE && m_bassOnsetHistory.size() >= MIN_HISTORY_SIZE;

    float globalOnset = 0.0f;
    float bassOnset = 0.0f;

    if (enoughHistory) {
        const float fluxMean = average(m_fluxHistory);
        const float fluxStd = standardDeviation(m_fluxHistory, fluxMean);

        if (flux > fluxMean) {
            globalOnset = (flux - fluxMean) / std::max(fluxStd, EPSILON);
        }

        const float bassMean = average(m_bassOnsetHistory);
        const float bassStd = standardDeviation(m_bassOnsetHistory, bassMean);

        if (bassRiseDb > bassMean) {
            bassOnset = (bassRiseDb - bassMean) / std::max(bassStd, EPSILON);
        }
    }

    const float energyGate = clamp01((result.rmsDb - ONSET_ENERGY_FLOOR_DB) / (ONSET_ENERGY_FULL_DB - ONSET_ENERGY_FLOOR_DB));

    globalOnset *= energyGate;
    bassOnset *= energyGate;

    result.globalOnsetStrength = globalOnset;
    result.bassOnsetStrength = bassOnset;
    result.onsetStrength = std::max(globalOnset, bassOnset);

    const bool localPeak = isLocalOnsetPeak(result.onsetStrength);
    const float onsetEvidence = clamp01(result.onsetStrength / std::max(m_beatSensitivity, EPSILON));

    result.beatScore = clamp01(onsetEvidence * (localPeak ? 1.0f : 0.55f));
    result.beat = false;

    const bool cooldownPassed = m_beatTimes.empty() || (m_timeSeconds - m_beatTimes.back()) >= (static_cast<double>(m_beatCooldownMs) / 1000.0);
    const bool strongOnset = result.onsetStrength >= m_beatSensitivity;
    const bool timingValid = m_currentBpm <= 0.0f || isBeatTimingValid(m_timeSeconds);

    if (strongOnset && localPeak && cooldownPassed && timingValid) {
        result.beat = true;
        m_beatTimes.push_back(m_timeSeconds);

        if (m_beatTimes.size() > BPM_HISTORY_SIZE + 2) {
            m_beatTimes.pop_front();
        }

        if (m_beatTimes.size() >= 2) {
            const double interval = m_beatTimes.back() - m_beatTimes[m_beatTimes.size() - 2];
            if (interval > 0.0) {
                result.beatInterval = static_cast<float>(interval);
                updateTempo(result.beatInterval, result);
            }
        }
    }

    m_previousSpectrum = result.spectrum;
    m_previousBassLevel = currentBassLevel;

    m_fluxHistory.push_back(flux);
    if (m_fluxHistory.size() > FLUX_HISTORY_SIZE) {
        m_fluxHistory.pop_front();
    }

    m_bassOnsetHistory.push_back(bassRiseDb);
    if (m_bassOnsetHistory.size() > BASS_ONSET_HISTORY_SIZE) {
        m_bassOnsetHistory.pop_front();
    }

    m_onsetHistory.push_back(result.onsetStrength);
    if (m_onsetHistory.size() > ONSET_PEAK_HISTORY_SIZE) {
        m_onsetHistory.pop_front();
    }

    result.bpm = m_currentBpm;
}

void AudioAnalyzer::updateTempo(float beatInterval, AudioAnalysis& result) {
    if (beatInterval <= 0.0f)
        return;

    const float rawBpm = 60.0f / beatInterval;
    std::vector<float> candidates;
    candidates.reserve(8);

    for (int multiple = 1; multiple <= MAX_BEAT_MULTIPLE; ++multiple) {
        const float candidate = rawBpm * static_cast<float>(multiple);
        if (candidate >= m_bpmMin && candidate <= m_bpmMax) {
            candidates.push_back(candidate);
        }

        const float halfCandidate = candidate * 0.5f;
        if (halfCandidate >= m_bpmMin && halfCandidate <= m_bpmMax) {
            candidates.push_back(halfCandidate);
        }
    }

    if (candidates.empty())
        return;

    if (m_currentBpm <= 0.0f) {
        float selected = rawBpm;
        bool validDirect = selected >= m_bpmMin && selected <= m_bpmMax;

        if (!validDirect) {
            float bestDistance = std::numeric_limits<float>::max();
            selected = 0.0f;

            for (const float candidate : candidates) {
                const float distance = std::fabs(candidate - rawBpm);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    selected = candidate;
                }
            }
        }

        if (selected <= 0.0f)
            return;

        m_tempoHistory.push_back(selected);
        if (m_tempoHistory.size() > BPM_HISTORY_SIZE) {
            m_tempoHistory.pop_front();
        }

        if (m_tempoHistory.size() >= 3) {
            m_currentBpm = median(m_tempoHistory);
        } else {
            m_currentBpm = selected;
        }

        result.bpm = m_currentBpm;
        return;
    }

    float bestCandidate = 0.0f;
    float bestDistance = std::numeric_limits<float>::max();

    for (const float candidate : candidates) {
        const float relativeDistance = std::fabs(candidate - m_currentBpm) / std::max(m_currentBpm, EPSILON);

        if (relativeDistance > TEMPO_TOLERANCE) {
            continue;
        }

        if (relativeDistance < bestDistance) {
            bestDistance = relativeDistance;
            bestCandidate = candidate;
        }
    }

    if (bestCandidate <= 0.0f)
        return;

    m_tempoHistory.push_back(bestCandidate);
    if (m_tempoHistory.size() > BPM_HISTORY_SIZE) {
        m_tempoHistory.pop_front();
    }

    m_currentBpm = median(m_tempoHistory);
    result.bpm = m_currentBpm;
}

bool AudioAnalyzer::isLocalOnsetPeak(float onset) const {
    if (onset < m_beatSensitivity) {
        return false;
    }

    if (m_onsetHistory.empty())
        return true;

    float recentMaximum = 0.0f;
    for (const float previous : m_onsetHistory) {
        recentMaximum = std::max(recentMaximum, previous);
    }

    return onset >= recentMaximum;
}

bool AudioAnalyzer::isBeatTimingValid(double currentTime) const {
    if (m_beatTimes.empty())
        return true;

    const double interval = currentTime - m_beatTimes.back();
    if (interval <= 0.0)
        return false;

    const double minimumInterval = static_cast<double>(m_beatCooldownMs) / 1000.0;
    if (interval < minimumInterval) {
        return false;
    }

    if (m_currentBpm <= 0.0f)
        return true;

    const double expectedPeriod = 60.0 / static_cast<double>(m_currentBpm);
    if (expectedPeriod <= 0.0)
        return false;

    const double rawMultiple = interval / expectedPeriod;
    int nearestMultiple = static_cast<int>(std::llround(rawMultiple));
    nearestMultiple = std::clamp(nearestMultiple, 1, MAX_BEAT_MULTIPLE);

    const double expectedInterval = expectedPeriod * static_cast<double>(nearestMultiple);
    const double error = std::fabs(interval - expectedInterval);
    const double allowedError = expectedPeriod * PHASE_TOLERANCE;

    return error <= allowedError;
}

float AudioAnalyzer::smooth(float current, float previous, float deltaMs) const {
    const float timeConstant = current > previous ? m_smoothingAttackMs : m_smoothingReleaseMs;
    const float alpha = 1.0f - std::exp(-deltaMs / std::max(timeConstant, 0.001f));
    return previous + (current - previous) * alpha;
}

float AudioAnalyzer::normalizeDb(float db) const {
    const float normalized = (db - m_spectrumFloorDb) / (m_spectrumCeilingDb - m_spectrumFloorDb);
    return clamp01(normalized);
}

float AudioAnalyzer::safeDb(float value) {
    return 20.0f * std::log10(std::max(std::fabs(value), EPSILON));
}

float AudioAnalyzer::safePowerDb(float value) {
    return 10.0f * std::log10(std::max(value, EPSILON));
}

float AudioAnalyzer::clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float AudioAnalyzer::average(const std::deque<float>& values) {
    if (values.empty())
        return 0.0f;

    float sum = 0.0f;
    for (const float value : values) {
        sum += value;
    }
    return sum / static_cast<float>(values.size());
}

float AudioAnalyzer::standardDeviation(const std::deque<float>& values, float mean) {
    if (values.size() < 2)
        return 0.0f;

    float sum = 0.0f;
    for (const float value : values) {
        const float difference = value - mean;
        sum += difference * difference;
    }
    return std::sqrt(sum / static_cast<float>(values.size() - 1));
}

float AudioAnalyzer::median(const std::deque<float>& values) {
    if (values.empty())
        return 0.0f;

    std::vector<float> sorted(values.begin(), values.end());
    std::sort(sorted.begin(), sorted.end());

    const size_t size = sorted.size();
    if (size & 1) {
        return sorted[size / 2];
    }

    return (sorted[size / 2 - 1] + sorted[size / 2]) * 0.5f;
}