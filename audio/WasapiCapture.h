#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#include <cstdint>
#include <memory>

#include "AudioAnalyzer.h"
#include "AudioDeviationAnalyzer.h"
#include "MusicPush.h"

class AudioBuffer;

class WasapiCapture
{
public:
    const MusicPush::Data& getMusicPush() const noexcept
    {
        return musicPush.get();
    }
    

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

    std::unique_ptr<AudioDeviationAnalyzer>
        deviationAnalyzer;

    AudioAnalysis latestAnalysis;
    AudioDeviation latestDeviation;
    MusicPush musicPush;

    void cleanup();

    void printFormat() const;

    void printAnalysis(
        uint64_t packetCount
    ) const;
};