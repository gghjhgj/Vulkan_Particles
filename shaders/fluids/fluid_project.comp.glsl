#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(r32f, binding = 0) readonly uniform image2D inPressure;
layout(rg32f, binding = 1) readonly uniform image2D inVelocity;
layout(rg32f, binding = 2) writeonly uniform image2D outVelocity;

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
    return imageLoad(inPressure, p).r;
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= int(push.simWidth) || pos.y >= int(push.simHeight)) return;

    float pL = getP(pos + ivec2(-1, 0));
    float pR = getP(pos + ivec2(1, 0));
    float pB = getP(pos + ivec2(0, -1));
    float pT = getP(pos + ivec2(0, 1));

    vec2 gradP = vec2(pR - pL, pT - pB) * 0.5;

    vec2 currentV = imageLoad(inVelocity, pos).xy;
    vec2 newV = currentV - gradP;

    imageStore(outVelocity, pos, vec4(newV, 0.0, 0.0));
}