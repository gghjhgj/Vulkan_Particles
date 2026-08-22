#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(r16f, binding = 0) uniform image2D imgPressure;
layout(r16f, binding = 1) readonly uniform image2D imgDivergence;

layout(push_constant) uniform Push
{
    float mouseX;
    float mouseY;
    float prevMouseX;
    float prevxMouseY;
    float dt;
    float splatRadius;
    float splatForce;
    float velocityDissipation;
    float densityDissipation;
    float vorticity;        
    uint simWidth;          
    uint simHeight;         
    uint windowWidth;       
    uint windowHeight;      
    uint isMouseDown;
    uint offsetFromLeft;
    uint offsetFromRight;
    float omega;
    uint pressureIterations;
} push;

shared float tileP[18][19];

void main()
{
    uint leftBound = push.offsetFromLeft >> 1;
    uint rightBound = (push.simWidth > push.offsetFromRight) ? ((push.simWidth - push.offsetFromRight) >> 1) : 0;

    if (rightBound <= leftBound) return;

    uint minGroupX = gl_WorkGroupID.x * 16;
    uint maxGroupX = minGroupX + 16;

    if (maxGroupX <= leftBound || minGroupX >= rightBound)
    {
        return;
    }

    ivec2 groupOrigin = ivec2(gl_WorkGroupID.xy) * 16;
    ivec2 localID = ivec2(gl_LocalInvocationID.xy);
    ivec2 pressSize = imageSize(imgPressure);
    ivec2 minBound = ivec2(int(leftBound), 0);
    ivec2 maxBound = ivec2(int(rightBound) - 1, pressSize.y - 1);

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

    bool inBounds = (globalPos.x >= leftBound) && (globalPos.x < rightBound) && 
                    (globalPos.x < pressSize.x) && (globalPos.y < pressSize.y);

    float div = inBounds ? imageLoad(imgDivergence, globalPos).r : 0.0;
    int parity = (globalPos.x + globalPos.y) & 1;

    for (uint iter = 0; iter < push.pressureIterations; ++iter)
    {
        if (inBounds && (parity == 0))
        {
            float pL = (globalPos.x > int(leftBound)) ? tileP[ly][lx - 1] : tileP[ly][lx];
            float pR = (globalPos.x < int(rightBound) - 1 && globalPos.x < pressSize.x - 1) ? tileP[ly][lx + 1] : tileP[ly][lx];
            float pB = (globalPos.y > 0) ? tileP[ly - 1][lx] : tileP[ly][lx];
            float pT = (globalPos.y < pressSize.y - 1) ? tileP[ly + 1][lx] : tileP[ly][lx];
            
            float pOld = tileP[ly][lx];
            float pNew = (pL + pR + pB + pT - div) * 0.25;

            tileP[ly][lx] = pOld + push.omega * (pNew - pOld);
        }

        barrier();

        if (inBounds && (parity == 1))
        {
            float pL = (globalPos.x > int(leftBound)) ? tileP[ly][lx - 1] : tileP[ly][lx];
            float pR = (globalPos.x < int(rightBound) - 1 && globalPos.x < pressSize.x - 1) ? tileP[ly][lx + 1] : tileP[ly][lx];
            float pB = (globalPos.y > 0) ? tileP[ly - 1][lx] : tileP[ly][lx];
            float pT = (globalPos.y < pressSize.y - 1) ? tileP[ly + 1][lx] : tileP[ly][lx];
            
            float pOld = tileP[ly][lx];
            float pNew = (pL + pR + pB + pT - div) * 0.25;

            tileP[ly][lx] = pOld + push.omega * (pNew - pOld);
        }

        barrier();
    }

    if (!inBounds) return;

    imageStore(imgPressure, globalPos, vec4(tileP[ly][lx], 0.0, 0.0, 0.0));
}