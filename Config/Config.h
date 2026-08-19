#pragma once

#include <cstdint>
#include <string>

struct WindowConfig
{
    int width;
    int height;
};

struct ParticlesConfig
{
    uint32_t count;
    float trail_length;
    float trail_width;
    float size;
};

struct FluidConfig {
    uint32_t simWidth = 512;
    uint32_t simHeight = 512;
    float velocityDissipation = 0.985f;
    float densityDissipation = 0.965f;
    float vorticity = 35.0f;
    uint32_t pressureIterations = 20;
    float splatRadius = 0.004f;
    float splatForce = 5000.0f;
};

class Config
{
public:
    static WindowConfig window;
    static ParticlesConfig particles;
    static FluidConfig fluid;

    static void load(const std::string& path);
};