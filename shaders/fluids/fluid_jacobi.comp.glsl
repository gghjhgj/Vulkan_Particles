#version 450
#extension GL_GOOGLE_include_directive : require

#include "../common/fluid_push.glsl"

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(r16f, binding = 0) uniform image2D imgPressure;
layout(r16f, binding = 1) readonly uniform image2D imgDivergence;

shared float tileP[18][19];

void main()
{
    float scaleX = float(push.pressWidth) / 1920.0;
    float scaleY = float(push.pressHeight) / 1080.0;

    uint leftBound = uint(float(push.offsetFromLeft) * scaleX);
    uint rightOffset = uint(float(push.offsetFromRight) * scaleX);
    uint rightBound = (push.pressWidth > rightOffset) ? (push.pressWidth - rightOffset) : 0;

    uint upBound = uint(float(push.offsetFromUp) * scaleY);
    uint downOffset = uint(float(push.offsetFromDown) * scaleY);
    uint downBound = (push.pressHeight > downOffset) ? (push.pressHeight - downOffset) : 0;

    if (rightBound <= leftBound || downBound <= upBound) return;

    uint minGroupX = gl_WorkGroupID.x * 16;
    uint maxGroupX = minGroupX + 16;
    uint minGroupY = gl_WorkGroupID.y * 16;
    uint maxGroupY = minGroupY + 16;

    if (maxGroupX <= leftBound || minGroupX >= rightBound || maxGroupY <= upBound || minGroupY >= downBound)
        return;

    ivec2 groupOrigin = ivec2(gl_WorkGroupID.xy) * 16;
    ivec2 localID = ivec2(gl_LocalInvocationID.xy);
    ivec2 minBound = ivec2(int(leftBound), int(upBound));
    ivec2 maxBound = ivec2(int(rightBound) - 1, int(downBound) - 1);

    uint linearTid = localID.y * 16 + localID.x;

    ivec2 samplePos0 = groupOrigin + ivec2(linearTid % 18, linearTid / 18) - ivec2(1);
    samplePos0 = clamp(samplePos0, minBound, maxBound);
    tileP[linearTid / 18][linearTid % 18] = imageLoad(imgPressure, samplePos0).r;

    if (linearTid < 68)
    {
        uint extraIdx = linearTid + 256;
        ivec2 samplePos1 = groupOrigin + ivec2(extraIdx % 18, extraIdx / 18) - ivec2(1);
        samplePos1 = clamp(samplePos1, minBound, maxBound);
        tileP[extraIdx / 18][extraIdx % 18] = imageLoad(imgPressure, samplePos1).r;
    }

    barrier();

    ivec2 globalPos = groupOrigin + localID;
    int lx = localID.x + 1;
    int ly = localID.y + 1;

    bool inBounds = (globalPos.x >= int(leftBound)) && (globalPos.x < int(rightBound)) && 
                    (globalPos.y >= int(upBound)) && (globalPos.y < int(downBound)) &&
                    (globalPos.x < int(push.pressWidth)) && (globalPos.y < int(push.pressHeight));

    float div = inBounds ? imageLoad(imgDivergence, globalPos).r : 0.0;
    int parity = (globalPos.x + globalPos.y) & 1;

    for (uint iter = 0; iter < push.pressureSteps; ++iter)
    {
        if (inBounds && (parity == 0))
        {
            float pL = (globalPos.x > int(leftBound)) ? tileP[ly][lx - 1] : tileP[ly][lx];
            float pR = (globalPos.x < int(rightBound) - 1 && globalPos.x < int(push.pressWidth) - 1) ? tileP[ly][lx + 1] : tileP[ly][lx];
            float pB = (globalPos.y > int(upBound)) ? tileP[ly - 1][lx] : tileP[ly][lx];
            float pT = (globalPos.y < int(downBound) - 1 && globalPos.y < int(push.pressHeight) - 1) ? tileP[ly + 1][lx] : tileP[ly][lx];
            
            float pOld = tileP[ly][lx];
            float pNew = (pL + pR + pB + pT - div) * 0.25;
            tileP[ly][lx] = pOld + push.omega * (pNew - pOld);
        }

        barrier();

        if (inBounds && (parity == 1))
        {
            float pL = (globalPos.x > int(leftBound)) ? tileP[ly][lx - 1] : tileP[ly][lx];
            float pR = (globalPos.x < int(rightBound) - 1 && globalPos.x < int(push.pressWidth) - 1) ? tileP[ly][lx + 1] : tileP[ly][lx];
            float pB = (globalPos.y > int(upBound)) ? tileP[ly - 1][lx] : tileP[ly][lx];
            float pT = (globalPos.y < int(downBound) - 1 && globalPos.y < int(push.pressHeight) - 1) ? tileP[ly + 1][lx] : tileP[ly][lx];
            
            float pOld = tileP[ly][lx];
            float pNew = (pL + pR + pB + pT - div) * 0.25;
            tileP[ly][lx] = pOld + push.omega * (pNew - pOld);
        }

        barrier();
    }

    if (!inBounds) return;

    imageStore(imgPressure, globalPos, vec4(tileP[ly][lx], 0.0, 0.0, 0.0));
}