#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#include <cstdint>
#include <memory>

#include "AudioAnalyzer.h"

class AudioBuffer;

class WasapiCapture
{
public:
    WasapiCapture();
    ~WasapiCapture();

    bool init();
    void run();

private:
    IMMDeviceEnumerator* deviceEnumerator =
        nullptr;

    IMMDevice* device =
        nullptr;

    IAudioClient* audioClient =
        nullptr;

    IAudioCaptureClient* captureClient =
        nullptr;

    WAVEFORMATEX* waveFormat =
        nullptr;

    bool comInitialized =
        false;

    std::unique_ptr<AudioBuffer>
        audioBuffer;

    std::unique_ptr<AudioAnalyzer>
        analyzer;

    AudioAnalysis latestAnalysis;

    void cleanup();

    void printFormat() const;

    void printAnalysis(
        uint64_t packetCount
    ) const;
};