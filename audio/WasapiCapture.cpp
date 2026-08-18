#include "WasapiCapture.h"

#include "AudioBuffer.h"
#include "AudioConfig/AudioConfig.h"
#include "AudioDeviationAnalyzer.h"

#include <vector>

WasapiCapture::WasapiCapture()
{
}

WasapiCapture::~WasapiCapture()
{
    cleanup();
}

bool WasapiCapture::init()
{
    HRESULT hr;

    hr =
        CoInitializeEx(
            nullptr,
            COINIT_MULTITHREADED);

    if (FAILED(hr))
        return false;

    comInitialized = true;

    hr =
        CoCreateInstance(
            __uuidof(
                MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            __uuidof(
                IMMDeviceEnumerator),
            reinterpret_cast<void **>(
                &deviceEnumerator));

    if (FAILED(hr))
        return false;

    hr =
        deviceEnumerator
            ->GetDefaultAudioEndpoint(
                eRender,
                eConsole,
                &device);

    if (FAILED(hr))
        return false;

    hr =
        device->Activate(
            __uuidof(
                IAudioClient),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void **>(
                &audioClient));

    if (FAILED(hr))
        return false;

    hr =
        audioClient->GetMixFormat(
            &waveFormat);

    if (FAILED(hr))
        return false;

    audioBuffer =
        std::make_unique<AudioBuffer>(
            waveFormat->nSamplesPerSec,
            waveFormat->nChannels,
            AudioConfig::audio.history_ms);

    analyzer =
        std::make_unique<AudioAnalyzer>(
            AudioConfig::audio.fft_size,
            waveFormat->nSamplesPerSec,

            AudioConfig::audio
                .smoothing_attack_ms,

            AudioConfig::audio
                .smoothing_release_ms,

            AudioConfig::audio
                .spectrum_floor_db,

            AudioConfig::audio
                .spectrum_ceiling_db,

            AudioConfig::audio
                .spectral_rolloff_percent,

            AudioConfig::audio
                .beat_sensitivity,

            AudioConfig::audio
                .beat_cooldown_ms,

            AudioConfig::audio
                .bpm_min,

            AudioConfig::audio
                .bpm_max);

    deviationAnalyzer =
        std::make_unique<AudioDeviationAnalyzer>();

    const REFERENCE_TIME
        bufferDuration =
            static_cast<
                REFERENCE_TIME>(
                AudioConfig::audio
                    .buffer_ms) *
            10000;

    hr =
        audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,

            AUDCLNT_STREAMFLAGS_LOOPBACK,

            bufferDuration,

            0,

            waveFormat,

            nullptr);

    if (FAILED(hr))
        return false;

    hr =
        audioClient->GetService(
            __uuidof(
                IAudioCaptureClient),
            reinterpret_cast<void **>(
                &captureClient));

    if (FAILED(hr))
        return false;

    hr =
        audioClient->Start();

    if (FAILED(hr))
        return false;

    return true;
}

void WasapiCapture::run()
{
    if (
        !captureClient ||
        !audioBuffer ||
        !analyzer ||
        !deviationAnalyzer)
    {
        return;
    }

    while (true)
    {
        UINT32 packetLength = 0;

        HRESULT hr =
            captureClient
                ->GetNextPacketSize(
                    &packetLength);

        if (FAILED(hr))
            break;

        if (packetLength == 0)
        {
            Sleep(1);
            continue;
        }

        while (
            packetLength > 0)
        {
            BYTE *data =
                nullptr;

            UINT32 numFrames =
                0;

            DWORD flags =
                0;

            hr =
                captureClient->GetBuffer(
                    &data,
                    &numFrames,
                    &flags,
                    nullptr,
                    nullptr);

            if (FAILED(hr))
                return;

            const size_t sampleCount =
                static_cast<size_t>(
                    numFrames) *
                waveFormat->nChannels;

            if (
                flags &
                AUDCLNT_BUFFERFLAGS_SILENT)
            {
                std::vector<float>
                    silence(
                        sampleCount,
                        0.0f);

                audioBuffer->push(
                    silence.data(),
                    silence.size());
            }
            else
            {
                const float *
                    samples =
                        reinterpret_cast<
                            const float *>(
                            data);

                audioBuffer->push(
                    samples,
                    sampleCount);
            }

            latestAnalysis =
                analyzer->analyze(
                    *audioBuffer,
                    numFrames);

            latestDeviation =
                deviationAnalyzer->analyze(
                    latestAnalysis,
                    static_cast<float>(
                        numFrames) /
                    static_cast<float>(
                        waveFormat
                            ->nSamplesPerSec) *
                    1000.0f);

            musicPush.update(
                latestAnalysis,
                latestDeviation,
                waveFormat->nSamplesPerSec);

            hr =
                captureClient
                    ->ReleaseBuffer(
                        numFrames);

            if (FAILED(hr))
                return;

            hr =
                captureClient
                    ->GetNextPacketSize(
                        &packetLength);

            if (FAILED(hr))
                return;
        }
    }
}

void WasapiCapture::cleanup()
{
    if (audioClient)
    {
        audioClient->Stop();
    }

    deviationAnalyzer.reset();

    analyzer.reset();

    audioBuffer.reset();

    if (waveFormat)
    {
        CoTaskMemFree(
            waveFormat);

        waveFormat =
            nullptr;
    }

    if (captureClient)
    {
        captureClient->Release();

        captureClient =
            nullptr;
    }

    if (audioClient)
    {
        audioClient->Release();

        audioClient =
            nullptr;
    }

    if (device)
    {
        device->Release();

        device =
            nullptr;
    }

    if (deviceEnumerator)
    {
        deviceEnumerator->Release();

        deviceEnumerator =
            nullptr;
    }

    if (comInitialized)
    {
        CoUninitialize();

        comInitialized =
            false;
    }
}