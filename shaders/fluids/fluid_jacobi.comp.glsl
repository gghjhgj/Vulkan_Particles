#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(std430, binding = 0) buffer PressureBuffer { float pressure[]; };
layout(std430, binding = 1) readonly buffer DivIn { float inDivergence[]; };

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

float getP(int x, int y)
{
    x = clamp(x, 0, int(push.simWidth - 1));
    y = clamp(y, 0, int(push.simHeight - 1));
    return pressure[y * int(push.simWidth) + x]; 
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    int y = pos.y;
    int x = pos.x * 2 + int((y + int(push.phase)) & 1);

    if (x >= int(push.simWidth) || y >= int(push.simHeight)) return;

    uint id = uint(y) * push.simWidth + uint(x);

    float pL = getP(x - 1, y);
    float pR = getP(x + 1, y);
    float pB = getP(x, y - 1);
    float pT = getP(x, y + 1);

    float div = inDivergence[id];

    float pOld = pressure[id];

    float pNew = (pL + pR + pB + pT - div) * 0.25;

    const float omega = 1.2;

    pressure[id] = pOld + omega * (pNew - pOld);
}