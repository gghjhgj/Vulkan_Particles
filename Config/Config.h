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

class Config
{
public:
    static WindowConfig window;
    static ParticlesConfig particles;

    static void load(const std::string& path);
};