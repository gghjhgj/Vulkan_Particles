#ifndef FLUID_PUSH_GLSL
#define FLUID_PUSH_GLSL

layout(push_constant) uniform FluidPush
{
    float mouseX, mouseY, prevMouseX, prevMouseY;
    float dt, splatRadius, splatForce;
    float velocityDissipation, densityDissipation, vorticity;
    uint renderWidth, renderHeight;
    uint simWidth, simHeight;
    uint pressWidth, pressHeight;
    uint windowWidth, windowHeight;
    uint isMouseDown;
    uint offsetFromLeft, offsetFromRight, offsetFromUp, offsetFromDown;
    float omega;
    uint pressureSteps;
} push;

#endif