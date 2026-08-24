#ifndef PARTICLE_GLSL
#define PARTICLE_GLSL

struct Particle
{
    float x;
    float y;

    float prevX;
    float prevY;

    float vx;
    float vy;

    uint color;
};

#endif