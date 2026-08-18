#include "AudioBuffer.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

AudioBuffer::AudioBuffer(uint32_t sampleRate, uint32_t channels, uint32_t historyMs)
    : m_sampleRate(sampleRate),
      m_channels(channels)
{
    if (sampleRate == 0) {
        throw std::invalid_argument("AudioBuffer: sample rate cannot be zero");
    }

    if (channels == 0) {
        throw std::invalid_argument("AudioBuffer: channels cannot be zero");
    }

    if (historyMs == 0) {
        throw std::invalid_argument("AudioBuffer: history cannot be zero");
    }

    const size_t frameCount = static_cast<size_t>(sampleRate) * historyMs / 1000;
    const size_t sampleCount = frameCount * channels;

    if (sampleCount == 0) {
        throw std::invalid_argument("AudioBuffer: calculated capacity is zero");
    }

    m_buffer.resize(sampleCount);
}

void AudioBuffer::push(const float* samples, size_t sampleCount) {
    if (!samples || sampleCount == 0)
        return;

    std::lock_guard lock(m_mutex);

    const size_t capacity = m_buffer.size();

    if (sampleCount >= capacity) {
        samples += sampleCount - capacity;
        sampleCount = capacity;
    }

    const size_t firstPart = std::min(sampleCount, capacity - m_writePosition);

    std::memcpy(m_buffer.data() + m_writePosition, samples, firstPart * sizeof(float));

    const size_t secondPart = sampleCount - firstPart;

    if (secondPart > 0) {
        std::memcpy(m_buffer.data(), samples + firstPart, secondPart * sizeof(float));
    }

    m_writePosition = (m_writePosition + sampleCount) % capacity;
    m_availableSamples = std::min(m_availableSamples + sampleCount, capacity);
}

size_t AudioBuffer::readLatest(float* destination, size_t sampleCount) const {
    if (!destination || sampleCount == 0) {
        return 0;
    }

    std::lock_guard lock(m_mutex);

    const size_t count = std::min(sampleCount, m_availableSamples);
    if (count == 0)
        return 0;

    const size_t capacity = m_buffer.size();
    const size_t start = (m_writePosition + capacity - count) % capacity;
    const size_t firstPart = std::min(count, capacity - start);

    std::memcpy(destination, m_buffer.data() + start, firstPart * sizeof(float));

    const size_t secondPart = count - firstPart;
    if (secondPart > 0) {
        std::memcpy(destination + firstPart, m_buffer.data(), secondPart * sizeof(float));
    }

    return count;
}

size_t AudioBuffer::availableSamples() const {
    std::lock_guard lock(m_mutex);
    return m_availableSamples;
}

size_t AudioBuffer::capacitySamples() const {
    return m_buffer.size();
}

uint32_t AudioBuffer::sampleRate() const {
    return m_sampleRate;
}

uint32_t AudioBuffer::channels() const {
    return m_channels;
}