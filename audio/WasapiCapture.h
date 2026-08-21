#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#include <cstdint>
#include <memory>
#include <atomic>

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
    void stop();

private:
    std::atomic<bool> running{false};
    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* audioClient = nullptr;
    IAudioCaptureClient* captureClient = nullptr;
    WAVEFORMATEX* waveFormat = nullptr;

    bool comInitialized = false;

    std::unique_ptr<AudioBuffer> audioBuffer;
    std::unique_ptr<AudioAnalyzer> analyzer;
    std::unique_ptr<AudioDeviationAnalyzer> deviationAnalyzer;

    AudioAnalysis latestAnalysis;
    AudioDeviation latestDeviation;
    MusicPush musicPush;

    void cleanup();
};