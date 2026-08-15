#pragma once

#include "RGB.h"
struct Particle
{
    float x;
    float y;

    float prevX;
    float prevY;
    
    float vx;
    float vy; 

    RGB color;
};