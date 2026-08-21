#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(r32f, binding = 0) uniform image2D imgPressure;
layout(r32f, binding = 1) readonly uniform image2D imgDivergence;

layout(push_constant) uniform Push
{
    float mouseX;
    float mouseY;
    float prevMouseX;
    float prevMouseY;
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
} push;

shared float tileP[18][18];

void main()
{
    ivec2 groupOrigin = ivec2(gl_WorkGroupID.xy) * 16;
    ivec2 localID = ivec2(gl_LocalInvocationID.xy);
    ivec2 pressSize = imageSize(imgPressure);

    uint linearTid = localID.y * 16 + localID.x;

    ivec2 samplePos0 = groupOrigin + ivec2(linearTid % 18, linearTid / 18) - ivec2(1);
    samplePos0 = clamp(samplePos0, ivec2(0), pressSize - ivec2(1));
    tileP[linearTid / 18][linearTid % 18] = imageLoad(imgPressure, samplePos0).r;

    if (linearTid < 68)
    {
        uint extraIdx = linearTid + 256;
        ivec2 samplePos1 = groupOrigin + ivec2(extraIdx % 18, extraIdx / 18) - ivec2(1);
        samplePos1 = clamp(samplePos1, ivec2(0), pressSize - ivec2(1));
        tileP[extraIdx / 18][extraIdx % 18] = imageLoad(imgPressure, samplePos1).r;
    }

    barrier();

    ivec2 globalPos = groupOrigin + localID;
    int lx = localID.x + 1;
    int ly = localID.y + 1;

    const float omega = 1.25;

    if (globalPos.x < pressSize.x && globalPos.y < pressSize.y)
    {
        if (((globalPos.x + globalPos.y) & 1) == 0)
        {
            float pL = tileP[ly][lx - 1];
            float pR = tileP[ly][lx + 1];
            float pB = tileP[ly - 1][lx];
            float pT = tileP[ly + 1][lx];

            float div = imageLoad(imgDivergence, globalPos).r;

            float pOld = tileP[ly][lx];
            float pNew = (pL + pR + pB + pT - div) * 0.25;

            tileP[ly][lx] = pOld + omega * (pNew - pOld);
        }
    }

    barrier();

    if (globalPos.x < pressSize.x && globalPos.y < pressSize.y)
    {
        if (((globalPos.x + globalPos.y) & 1) == 1)
        {
            float pL = tileP[ly][lx - 1];
            float pR = tileP[ly][lx + 1];
            float pB = tileP[ly - 1][lx];
            float pT = tileP[ly + 1][lx];

            float div = imageLoad(imgDivergence, globalPos).r;

            float pOld = tileP[ly][lx];
            float pNew = (pL + pR + pB + pT - div) * 0.25;

            tileP[ly][lx] = pOld + omega * (pNew - pOld);
        }
    }

    barrier();

    if (globalPos.x < pressSize.x && globalPos.y < pressSize.y)
    {
        imageStore(imgPressure, globalPos, vec4(tileP[ly][lx], 0.0, 0.0, 0.0));
    }
}