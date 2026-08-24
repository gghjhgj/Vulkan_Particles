#version 450
#extension GL_GOOGLE_include_directive : require

#include "../common/fluid_push.glsl"
#include "../common/math.glsl"

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D inPressure;
layout(binding = 1) uniform sampler2D inVelocity;
layout(binding = 2) uniform sampler2D inColor;

layout(rg16f, binding = 3) writeonly uniform image2D outVelocity;
layout(rgba8, set = 0, binding = 4) uniform image2D outColor;
layout(r16f, binding = 5) writeonly uniform image2D outDivergence;

shared vec2 s_vel[18][19];

vec2 getProjectedVelocity(ivec2 p, ivec2 minBound, ivec2 maxBound, vec2 invSim, vec2 invPress, uint leftBound, uint rightBound, uint upBound, uint downBound)
{
    p = clamp(p, minBound, maxBound);
    vec2 uv = (vec2(p) + 0.5) * invSim;

    float pCenter = texture(inPressure, uv).r;
    float pR = (p.x < int(rightBound) - 1) ? texture(inPressure, uv + vec2( invPress.x, 0.0)).r : pCenter;
    float pL = (p.x > int(leftBound))      ? texture(inPressure, uv + vec2(-invPress.x, 0.0)).r : pCenter;
    float pT = (p.y < int(downBound) - 1)  ? texture(inPressure, uv + vec2(0.0,  invPress.y)).r : pCenter;
    float pB = (p.y > int(upBound))        ? texture(inPressure, uv + vec2(0.0, -invPress.y)).r : pCenter;

    float scaleX = (p.x > int(leftBound) && p.x < int(rightBound) - 1) ? 0.5 : 1.0;
    float scaleY = (p.y > int(upBound)   && p.y < int(downBound) - 1)  ? 0.5 : 1.0;

    vec2 gradP = vec2((pR - pL) * scaleX, (pT - pB) * scaleY);

    const float MAX_PRESSURE_KICK = 500.0;
    float gradLenSq = dot(gradP, gradP);
    if (gradLenSq > MAX_PRESSURE_KICK * MAX_PRESSURE_KICK)
    {
        gradP *= (MAX_PRESSURE_KICK * inversesqrt(gradLenSq));
    }

    vec2 v = texelFetch(inVelocity, p, 0).xy - gradP;

    if (p.x <= int(leftBound))          v.x = max(0.0, v.x);
    if (p.x >= int(rightBound) - 1)     v.x = min(0.0, v.x);
    if (p.y <= int(upBound))            v.y = max(0.0, v.y);
    if (p.y >= int(downBound) - 1)      v.y = min(0.0, v.y);

    return applySoftSpeedLimitFast(v, 400.0, 800.0);
}

void main()
{
    float scaleX = float(push.simWidth) / 1920.0;
    float scaleY = float(push.simHeight) / 1080.0;

    uint leftBound = uint(float(push.offsetFromLeft) * scaleX);
    uint rightOffset = uint(float(push.offsetFromRight) * scaleX);
    uint rightBound = (push.simWidth > rightOffset) ? (push.simWidth - rightOffset) : 0;

    uint upBound = uint(float(push.offsetFromUp) * scaleY);
    uint downOffset = uint(float(push.offsetFromDown) * scaleY);
    uint downBound = (push.simHeight > downOffset) ? (push.simHeight - downOffset) : 0;

    if (rightBound <= leftBound || downBound <= upBound) return;

    ivec2 groupOrigin = ivec2(gl_WorkGroupID.xy) * 16;
    uint minGroupX = groupOrigin.x;
    uint maxGroupX = minGroupX + 16;
    uint minGroupY = groupOrigin.y;
    uint maxGroupY = minGroupY + 16;
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    if (maxGroupX <= leftBound || minGroupX >= rightBound || maxGroupY <= upBound || minGroupY >= downBound)
    {
        if (pos.x < int(push.simWidth) && pos.y < int(push.simHeight))
        {
            imageStore(outColor, pos, vec4(0.0));
        }
        return;
    }

    ivec2 minBound = ivec2(int(leftBound), int(upBound));
    ivec2 maxBound = ivec2(int(rightBound) - 1, int(downBound) - 1);
    
    vec2 invSim = 1.0 / vec2(float(push.simWidth), float(push.simHeight));
    vec2 invPress = 1.0 / vec2(float(push.pressWidth), float(push.pressHeight));
    uint linearTid = gl_LocalInvocationIndex;

    ivec2 p0 = groupOrigin + ivec2(linearTid % 18, linearTid / 18) - ivec2(1);
    s_vel[linearTid / 18][linearTid % 18] = getProjectedVelocity(p0, minBound, maxBound, invSim, invPress, leftBound, rightBound, upBound, downBound);

    if (linearTid < 68)
    {
        uint extraIdx = linearTid + 256;
        ivec2 p1 = groupOrigin + ivec2(extraIdx % 18, extraIdx / 18) - ivec2(1);
        s_vel[extraIdx / 18][extraIdx % 18] = getProjectedVelocity(p1, minBound, maxBound, invSim, invPress, leftBound, rightBound, upBound, downBound);
    }

    barrier();

    if (pos.x >= int(push.simWidth) || pos.y >= int(push.simHeight)) 
        return;

    if (pos.x < int(leftBound) || pos.x >= int(rightBound) || pos.y < int(upBound) || pos.y >= int(downBound))
    {
        imageStore(outColor, pos, vec4(0.0));
        return;
    }

    vec2 uv = (vec2(pos) + 0.5) * invSim;
    float dt = (push.dt > 0.0 && push.dt < 0.1) ? push.dt : 0.016;

    ivec2 localID = ivec2(gl_LocalInvocationID.xy);
    int lx = localID.x + 1;
    int ly = localID.y + 1;

    vec2 currentV = s_vel[ly][lx];
    vec2 traceUV = uv - (currentV * dt) * invSim;

    vec2 minUV = (vec2(float(leftBound), float(upBound)) + 0.5) * invSim;
    vec2 maxUV = (vec2(float(rightBound - 1), float(downBound - 1)) + 0.5) * invSim;
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
    float dTop    = float(pos.y - int(upBound));
    float dBottom = float(int(downBound) - 1 - pos.y);

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
    vec4 finalColor = max(advC * push.densityDissipation - vec4(0.5 / 255.0), vec4(0.0));

    imageStore(outVelocity, pos, vec4(advV * push.velocityDissipation, 0.0, 0.0));
    imageStore(outColor, pos, finalColor);

    uint stepX = max(1u, push.simWidth / push.pressWidth);
    uint stepY = max(1u, push.simHeight / push.pressHeight);

    if ((uint(pos.x) % stepX == 0) && (uint(pos.y) % stepY == 0))
    {
        ivec2 pressCoord = ivec2(uint(pos.x) / stepX, uint(pos.y) / stepY);

        if (pressCoord.x < int(push.pressWidth) && pressCoord.y < int(push.pressHeight))
        {
            vec2 sampleR = texture(inVelocity, clamp(uv + vec2(invPress.x, 0.0) - currentV * dt * invSim, minUV, maxUV)).xy;
            vec2 sampleL = texture(inVelocity, clamp(uv - vec2(invPress.x, 0.0) - currentV * dt * invSim, minUV, maxUV)).xy;
            vec2 sampleT = texture(inVelocity, clamp(uv + vec2(0.0, invPress.y) - currentV * dt * invSim, minUV, maxUV)).xy;
            vec2 sampleB = texture(inVelocity, clamp(uv - vec2(0.0, invPress.y) - currentV * dt * invSim, minUV, maxUV)).xy;

            float div = 0.5 * ((sampleR.x - sampleL.x) + (sampleT.y - sampleB.y));
            imageStore(outDivergence, pressCoord, vec4(clamp(div, -2000.0, 2000.0), 0.0, 0.0, 0.0));
        }
    }
}