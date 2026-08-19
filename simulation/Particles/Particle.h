#pragma once

#include "../RGBA.h"
struct Particle
{
    float x;
    float y;

    float prevX;
    float prevY;
    
    float vx;
    float vy; 

    RGBA color;
};