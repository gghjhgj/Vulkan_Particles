#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <mutex>

class AudioBuffer
{
public:
    AudioBuffer(
        uint32_t sampleRate,
        uint32_t channels,
        uint32_t historyMs
    );

    void push(
        const float* samples,
        size_t sampleCount
    );

    size_t readLatest(
        float* destination,
        size_t sampleCount
    ) const;

    size_t availableSamples() const;

    size_t capacitySamples() const;

    uint32_t sampleRate() const;
    uint32_t channels() const;

private:
    uint32_t m_sampleRate;
    uint32_t m_channels;

    std::vector<float> m_buffer;

    size_t m_writePosition = 0;
    size_t m_availableSamples = 0;

    mutable std::mutex m_mutex;
};