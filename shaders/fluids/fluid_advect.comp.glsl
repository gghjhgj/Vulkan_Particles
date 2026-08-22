#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D inVelocity;
layout(binding = 1) uniform sampler2D inColor;

layout(rg16f, binding = 2) writeonly uniform image2D outVelocity;
layout(rgba8, set = 0, binding = 3) uniform image2D outColor;
layout(r16f, binding = 4) writeonly uniform image2D outDivergence;

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
    uint offsetFromLeft;
    uint offsetFromRight;
} push;

shared vec2 s_vel[18][18];

vec2 applySoftSpeedLimitFast(vec2 v, float threshold, float hardMax)
{
    float speedSq = dot(v, v);
    if (speedSq > threshold * threshold)
    {
        float speed = sqrt(speedSq);
        float excess = speed - threshold;
        float maxExcess = hardMax - threshold;
        float compressed = threshold + maxExcess * (excess / (excess + maxExcess));
        return (v / speed) * compressed;
    }
    return v;
}

void main()
{
    uint leftBound = push.offsetFromLeft;
    uint rightBound = (push.simWidth > push.offsetFromRight) ? (push.simWidth - push.offsetFromRight) : 0;

    if (rightBound <= leftBound) return;

    ivec2 groupOrigin = ivec2(gl_WorkGroupID.xy) * 16;
    uint minGroupX = groupOrigin.x;
    uint maxGroupX = minGroupX + 16;

    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    if (maxGroupX <= leftBound || minGroupX >= rightBound)
    {
        if (pos.x < int(push.simWidth) && pos.y < int(push.simHeight))
        {
            imageStore(outColor, pos, vec4(0.0));
        }
        return;
    }

    ivec2 minBound = ivec2(int(leftBound), 0);
    ivec2 maxBound = ivec2(int(rightBound) - 1, int(push.simHeight) - 1);
    uint linearTid = gl_LocalInvocationIndex;

    ivec2 p0 = groupOrigin + ivec2(linearTid % 18, linearTid / 18) - ivec2(1);
    s_vel[linearTid / 18][linearTid % 18] = texelFetch(inVelocity, clamp(p0, minBound, maxBound), 0).xy;

    if (linearTid < 68)
    {
        uint extraIdx = linearTid + 256;
        ivec2 p1 = groupOrigin + ivec2(extraIdx % 18, extraIdx / 18) - ivec2(1);
        s_vel[extraIdx / 18][extraIdx % 18] = texelFetch(inVelocity, clamp(p1, minBound, maxBound), 0).xy;
    }

    barrier();

    if (pos.x >= int(push.simWidth) || pos.y >= int(push.simHeight)) 
        return;

    if (pos.x < int(leftBound) || pos.x >= int(rightBound))
    {
        imageStore(outColor, pos, vec4(0.0));
        return;
    }

    vec2 simSize = vec2(float(push.simWidth), float(push.simHeight));
    vec2 invSim = 1.0 / simSize;
    vec2 uv = (vec2(pos) + 0.5) * invSim;
    float dt = (push.dt > 0.0 && push.dt < 0.1) ? push.dt : 0.016;

    ivec2 localID = ivec2(gl_LocalInvocationID.xy);
    int lx = localID.x + 1;
    int ly = localID.y + 1;

    vec2 currentV = s_vel[ly][lx];
    vec2 traceUV = uv - (currentV * dt) * invSim;

    vec2 minUV = (vec2(float(leftBound), 0.0) + 0.5) * invSim;
    vec2 maxUV = (vec2(float(rightBound - 1), float(push.simHeight - 1)) + 0.5) * invSim;
    traceUV = clamp(traceUV, minUV, maxUV);

    vec2 advV = texture(inVelocity, traceUV).xy;
    vec4 advC = texture(inColor, traceUV);

    vec2 vL = s_vel[ly][lx - 1];
    vec2 vR = s_vel[ly][lx + 1];
    vec2 vB = s_vel[ly - 1][lx];
    vec2 vT = s_vel[ly + 1][lx];

    float curlCenter = (vR.y - vL.y) - (vT.x - vB.x);

    vec2 vTL = s_vel[ly + 1][lx - 1];
    vec2 vTR = s_vel[ly + 1][lx + 1];
    vec2 vBL = s_vel[ly - 1][lx - 1];
    vec2 vBR = s_vel[ly - 1][lx + 1];

    float curlL = abs((currentV.y - vL.y)       - (vTL.x - vBL.x) * 0.5);
    float curlR = abs((vR.y - currentV.y)       - (vTR.x - vBR.x) * 0.5);
    float curlB = abs((vBR.y - vBL.y) * 0.5     - (currentV.x - vB.x));
    float curlT = abs((vTR.y - vTL.y) * 0.5     - (vT.x - currentV.x));

    vec2 grad = vec2(curlR - curlL, curlT - curlB) * 0.5;
    float gradLenSq = dot(grad, grad);

    float dLeft   = float(pos.x - int(leftBound));
    float dRight  = float(int(rightBound) - 1 - pos.x);
    float dTop    = float(pos.y);
    float dBottom = float(int(push.simHeight) - 1 - pos.y);

    if (gradLenSq > 0.0000001 && dLeft > 6.0 && dRight > 6.0 && dTop > 6.0 && dBottom > 6.0)
    {
        float invGradLen = inversesqrt(gradLenSq);
        vec2 N = grad * invGradLen;
        vec2 force = vec2(N.y, -N.x) * (curlCenter * push.vorticity); 
        advV += force * dt;
    }

    float margin = 16.0;
    if (dLeft < margin && advV.x < 0.0)   advV.x *= smoothstep(0.0, 1.0, dLeft / margin);
    if (dRight < margin && advV.x > 0.0)  advV.x *= smoothstep(0.0, 1.0, dRight / margin);
    if (dTop < margin && advV.y < 0.0)    advV.y *= smoothstep(0.0, 1.0, dTop / margin);
    if (dBottom < margin && advV.y > 0.0) advV.y *= smoothstep(0.0, 1.0, dBottom / margin);

    advV = applySoftSpeedLimitFast(advV, 400.0, 800.0);
    
    vec4 finalColor = max(advC * push.densityDissipation - vec4(0.6 / 255.0), vec4(0.0));

    imageStore(outVelocity, pos, vec4(advV * push.velocityDissipation, 0.0, 0.0));
    imageStore(outColor, pos, finalColor);

    if ((localID.x & 1) == 0 && (localID.y & 1) == 0)
    {
        vec2 sR0 = s_vel[ly][lx + 2];
        vec2 sR1 = s_vel[ly + 1][lx + 2];
        vec2 sL0 = s_vel[ly][lx - 1];
        vec2 sL1 = s_vel[ly + 1][lx - 1];
        
        vec2 sT0 = s_vel[ly + 2][lx];
        vec2 sT1 = s_vel[ly + 2][lx + 1];
        vec2 sB0 = s_vel[ly - 1][lx];
        vec2 sB1 = s_vel[ly - 1][lx + 1];

        float fluxX = (sR0.x + sR1.x) - (sL0.x + sL1.x);
        float fluxY = (sT0.y + sT1.y) - (sB0.y + sB1.y);
        
        float div = 0.25 * (fluxX + fluxY);
        
        imageStore(outDivergence, pos >> 1, vec4(clamp(div, -2000.0, 2000.0), 0.0, 0.0, 0.0));
    }
}