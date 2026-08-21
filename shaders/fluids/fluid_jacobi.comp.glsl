#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// In-Place Read/Write Image
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
    uint phase;
} push;

float getP(ivec2 p)
{
    p = clamp(p, ivec2(0), ivec2(int(push.simWidth - 1), int(push.simHeight - 1)));
    return imageLoad(imgPressure, p).r;
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    int y = pos.y;
    int x = pos.x * 2 + int((y + int(push.phase)) & 1);

    if (x >= int(push.simWidth) || y >= int(push.simHeight)) return;

    ivec2 coord = ivec2(x, y);

    float pL = getP(coord + ivec2(-1, 0));
    float pR = getP(coord + ivec2(1, 0));
    float pB = getP(coord + ivec2(0, -1));
    float pT = getP(coord + ivec2(0, 1));

    float div = imageLoad(imgDivergence, coord).r;

    float pOld = imageLoad(imgPressure, coord).r;
    float pNew = (pL + pR + pB + pT - div) * 0.25;

    const float omega = 1.25;
    float pFinal = pOld + omega * (pNew - pOld);

    imageStore(imgPressure, coord, vec4(pFinal, 0.0, 0.0, 0.0));
}