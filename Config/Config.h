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
    uint32_t simWidth;
    uint32_t simHeight;
    float velocityDissipation;
    float densityDissipation;
    float vorticity;
    uint32_t pressureIterations;
    float splatRadius;
    float splatForce;
    uint32_t offsetFromRight;
    uint32_t offsetFromLeft;
    uint32_t offsetFromUp;
    uint32_t offsetFromDown;
    float omega;
    uint32_t pressureSteps;
    
    uint32_t colorScale = 1;
    uint32_t simScale = 2;
    uint32_t pressureScale = 4;
};

struct VisualsConfig {
    float sharpness;
    float highPassLimit;
    float normalStrength;
    float lightDirX;
    float lightDirY;
    float lightDirZ;
    float lightIntensity;
    float ambientLight;
    float colorBoost;
    float gamma;
    float exposure;
    float blurStrength;
};

class Config
{
public:
    static WindowConfig window;
    static ParticlesConfig particles;
    static FluidConfig fluid;
    static VisualsConfig visuals;

    static void load(const std::string& path);
};