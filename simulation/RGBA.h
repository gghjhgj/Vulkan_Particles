#pragma once
#include <cstdint>

struct RGBA {
    uint32_t value;

    RGBA(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255)
        : value(static_cast<uint32_t>(r) |
               (static_cast<uint32_t>(g) << 8) |
               (static_cast<uint32_t>(b) << 16) |
               (static_cast<uint32_t>(a) << 24)) {}

    uint8_t r() const { return value & 0xFF; }
    uint8_t g() const { return (value >> 8) & 0xFF; }
    uint8_t b() const { return (value >> 16) & 0xFF; }
    uint8_t a() const { return (value >> 24) & 0xFF; }

    void setR(uint8_t r) { value = (value & 0xFFFFFF00u) | r; }
    void setG(uint8_t g) { value = (value & 0xFFFF00FFu) | (static_cast<uint32_t>(g) << 8); }
    void setB(uint8_t b) { value = (value & 0xFF00FFFFu) | (static_cast<uint32_t>(b) << 16); }
    void setA(uint8_t a) { value = (value & 0x00FFFFFFu) | (static_cast<uint32_t>(a) << 24); }

    float rNorm() const { return r() / 255.0f; }
    float gNorm() const { return g() / 255.0f; }
    float bNorm() const { return b() / 255.0f; }
    float aNorm() const { return a() / 255.0f; }
};