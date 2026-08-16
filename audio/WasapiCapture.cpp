#include "WasapiCapture.h"

#include "AudioBuffer.h"
#include "AudioConfig/AudioConfig.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <stdexcept>

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
    {
        std::cerr
            << "CoInitializeEx failed: 0x"
            << std::hex
            << hr
            << '\n';

        return false;
    }

    comInitialized =
        true;

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
    {
        std::cerr
            << "CoCreateInstance failed: 0x"
            << std::hex
            << hr
            << '\n';

        return false;
    }

    hr =
        deviceEnumerator
            ->GetDefaultAudioEndpoint(
                eRender,
                eConsole,
                &device);

    if (FAILED(hr))
    {
        std::cerr
            << "GetDefaultAudioEndpoint failed: 0x"
            << std::hex
            << hr
            << '\n';

        return false;
    }

    hr =
        device->Activate(
            __uuidof(
                IAudioClient),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void **>(
                &audioClient));

    if (FAILED(hr))
    {
        std::cerr
            << "Activate IAudioClient failed: 0x"
            << std::hex
            << hr
            << '\n';

        return false;
    }

    hr =
        audioClient->GetMixFormat(
            &waveFormat);

    if (FAILED(hr))
    {
        std::cerr
            << "GetMixFormat failed: 0x"
            << std::hex
            << hr
            << '\n';

        return false;
    }

    printFormat();

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
    {
        std::cerr
            << "IAudioClient::Initialize failed: 0x"
            << std::hex
            << hr
            << '\n';

        return false;
    }

    hr =
        audioClient->GetService(
            __uuidof(
                IAudioCaptureClient),
            reinterpret_cast<void **>(
                &captureClient));

    if (FAILED(hr))
    {
        std::cerr
            << "GetService IAudioCaptureClient failed: 0x"
            << std::hex
            << hr
            << '\n';

        return false;
    }

    hr =
        audioClient->Start();

    if (FAILED(hr))
    {
        std::cerr
            << "IAudioClient::Start failed: 0x"
            << std::hex
            << hr
            << '\n';

        return false;
    }

    std::cout
        << "\nWASAPI LOOPBACK STARTED\n\n";

    std::cout
        << "Capture buffer : "
        << AudioConfig::audio.buffer_ms
        << " ms\n";

    std::cout
        << "Audio history  : "
        << AudioConfig::audio.history_ms
        << " ms\n";

    std::cout
        << "FFT size       : "
        << AudioConfig::audio.fft_size
        << '\n';

    std::cout
        << "Frequency bin  : "
        << static_cast<float>(
               waveFormat->nSamplesPerSec) /
               static_cast<float>(
                   AudioConfig::audio.fft_size)
        << " Hz\n";

    std::cout
        << "============================\n\n";

    return true;
}

void WasapiCapture::printFormat() const
{
    std::cout
        << "============================\n"
        << "WASAPI FORMAT\n"
        << "============================\n";

    std::cout
        << "Sample rate : "
        << waveFormat->nSamplesPerSec
        << " Hz\n";

    std::cout
        << "Channels    : "
        << waveFormat->nChannels
        << '\n';

    std::cout
        << "Bits/sample : "
        << waveFormat->wBitsPerSample
        << '\n';

    std::cout
        << "Block align : "
        << waveFormat->nBlockAlign
        << '\n';

    std::cout
        << "Avg bytes/s : "
        << waveFormat->nAvgBytesPerSec
        << '\n';

    std::cout
        << "Format tag  : 0x"
        << std::hex
        << waveFormat->wFormatTag
        << std::dec
        << '\n';

    if (
        waveFormat->wFormatTag ==
        WAVE_FORMAT_EXTENSIBLE)
    {
        auto *extensible =
            reinterpret_cast<
                WAVEFORMATEXTENSIBLE *>(
                waveFormat);

        std::cout
            << "Extensible format detected\n";

        std::cout
            << "Valid bits  : "
            << extensible
                   ->Samples
                   .wValidBitsPerSample
            << '\n';

        std::cout
            << "Channel mask: 0x"
            << std::hex
            << extensible
                   ->dwChannelMask
            << std::dec
            << '\n';
    }

    std::cout
        << "============================\n";
}

void WasapiCapture::printAnalysis(
    uint64_t packetCount) const
{
    const auto &a =
        latestAnalysis;

    std::cout
        << "Packet: "
        << packetCount

        << " | RMS: "
        << a.rms

        << " | RMS dB: "
        << a.rmsDb

        << " | Peak: "
        << a.peak

        << " | Energy dB: "
        << a.energyDb

        << " | Bass: "
        << a.bassLevel

        << " | Mid: "
        << a.midLevel

        << " | Treble: "
        << a.trebleLevel

        << " | Centroid: "
        << a.spectralCentroid
        << " Hz"

        << " | Rolloff: "
        << a.spectralRolloff
        << " Hz"

        << " | Flatness: "
        << a.spectralFlatness

        << " | Flux: "
        << a.spectralFlux

        << " | GlobalOnset: "
        << a.globalOnsetStrength

        << " | BassOnset: "
        << a.bassOnsetStrength

        << " | Onset: "
        << a.onsetStrength

        << " | Beat: "
        << (a.beat
                ? "YES"
                : "NO")

        << " | BeatScore: "
        << a.beatScore

        << " | Interval: "
        << a.beatInterval
        << " s"

        << " | BPM: "
        << a.bpm

        << '\n';
}

void WasapiCapture::run()
{
    if (
        !captureClient ||
        !audioBuffer ||
        !analyzer)
    {
        return;
    }

    std::cout
        << "Receiving audio...\n"
        << "Full analysis active.\n"
        << "Press CTRL+C to stop.\n\n";

    uint64_t totalFrames = 0;
    uint64_t packetCount = 0;

    while (true)
    {
        UINT32 packetLength = 0;

        HRESULT hr =
            captureClient
                ->GetNextPacketSize(
                    &packetLength);

        if (FAILED(hr))
        {
            std::cerr
                << "GetNextPacketSize failed: 0x"
                << std::hex
                << hr
                << '\n';

            break;
        }

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
            {
                std::cerr
                    << "GetBuffer failed: 0x"
                    << std::hex
                    << hr
                    << '\n';

                return;
            }

            ++packetCount;

            totalFrames +=
                numFrames;

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

            if (
                packetCount % 50 == 0 ||
                latestAnalysis.beat)
            {
                printAnalysis(
                    packetCount);
            }

            hr =
                captureClient
                    ->ReleaseBuffer(
                        numFrames);

            if (FAILED(hr))
            {
                std::cerr
                    << "ReleaseBuffer failed: 0x"
                    << std::hex
                    << hr
                    << '\n';

                return;
            }

            hr =
                captureClient
                    ->GetNextPacketSize(
                        &packetLength);

            if (FAILED(hr))
            {
                std::cerr
                    << "GetNextPacketSize failed: 0x"
                    << std::hex
                    << hr
                    << '\n';

                return;
            }
        }
    }
}

void WasapiCapture::cleanup()
{
    if (audioClient)
    {
        audioClient->Stop();
    }

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