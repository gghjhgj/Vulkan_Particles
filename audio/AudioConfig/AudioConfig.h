#pragma once

#include <cstdint>
#include <string>

struct AudioConfigData
{
    uint32_t sample_rate;
    uint32_t channels;

    uint32_t buffer_ms;

    uint32_t history_ms;

    uint32_t fft_size;

    float smoothing_attack_ms;
    float smoothing_release_ms;

    float spectrum_floor_db;
    float spectrum_ceiling_db;

    float spectral_rolloff_percent;

    float beat_sensitivity;
    uint32_t beat_cooldown_ms;

    float bpm_min;
    float bpm_max;
};

class AudioConfig
{
public:
    static AudioConfigData audio;

    static void load(
        const std::string& path
    );
};