#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D inVelocity;
layout(binding = 1) uniform sampler2D inColor;

layout(rg16f, binding = 2) writeonly uniform image2D outVelocity;
layout(rgba8, set = 0, binding = 3) uniform image2D outColor;
layout(r32f, binding = 4) writeonly uniform image2D outDivergence;

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

shared vec2 s_vel[18][18];

vec2 fetchV(ivec2 p, ivec2 maxBound)
{
    return texelFetch(inVelocity, clamp(p, ivec2(0), maxBound), 0).xy;
}

vec3 getVelocityColor(vec2 dir)
{
    float len = length(dir);
    if (len < 0.001) return vec3(0.0, 0.8, 1.0);
    float angle = atan(dir.y, dir.x);
    return 0.5 + 0.5 * cos(angle + vec3(0.0, 2.0, 4.0));
}

float distToSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a;
    vec2 ba = b - a;
    float d = dot(ba, ba);
    float h = (d > 0.00001) ? clamp(dot(pa, ba) / d, 0.0, 1.0) : 0.0;
    return length(pa - ba * h);
}

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    ivec2 localID = ivec2(gl_LocalInvocationID.xy);
    ivec2 maxBound = ivec2(int(push.simWidth) - 1, int(push.simHeight) - 1);

    s_vel[localID.x + 1][localID.y + 1] = fetchV(pos, maxBound);

    if (localID.x == 0)  s_vel[0][localID.y + 1]  = fetchV(pos + ivec2(-1,  0), maxBound);
    if (localID.x == 15) s_vel[17][localID.y + 1] = fetchV(pos + ivec2( 1,  0), maxBound);
    if (localID.y == 0)  s_vel[localID.x + 1][0]  = fetchV(pos + ivec2( 0, -1), maxBound);
    if (localID.y == 15) s_vel[localID.x + 1][17] = fetchV(pos + ivec2( 0,  1), maxBound);

    if (localID.x == 0  && localID.y == 0)  s_vel[0][0]   = fetchV(pos + ivec2(-1, -1), maxBound);
    if (localID.x == 15 && localID.y == 0)  s_vel[17][0]  = fetchV(pos + ivec2( 1, -1), maxBound);
    if (localID.x == 0  && localID.y == 15) s_vel[0][17]  = fetchV(pos + ivec2(-1,  1), maxBound);
    if (localID.x == 15 && localID.y == 15) s_vel[17][17] = fetchV(pos + ivec2( 1,  1), maxBound);

    barrier();

    if (pos.x > maxBound.x || pos.y > maxBound.y) return;

    vec2 simSize = vec2(float(push.simWidth), float(push.simHeight));
    vec2 invSim = 1.0 / simSize;
    vec2 uv = (vec2(pos) + 0.5) * invSim;

    float dt = (push.dt > 0.0 && push.dt < 0.1) ? push.dt : 0.016;

    int lx = localID.x + 1;
    int ly = localID.y + 1;

    vec2 currentV = s_vel[lx][ly];
    vec2 traceUV = uv - (currentV * dt) * invSim;
    vec2 advV = texture(inVelocity, traceUV).xy;
    vec4 advC = texture(inColor, traceUV);

    vec2 vL = s_vel[lx - 1][ly];
    vec2 vR = s_vel[lx + 1][ly];
    vec2 vB = s_vel[lx][ly - 1];
    vec2 vT = s_vel[lx][ly + 1];

    float curlCenter = (vR.y - vL.y) - (vT.x - vB.x);

    vec2 vTL = s_vel[lx - 1][ly + 1];
    vec2 vTR = s_vel[lx + 1][ly + 1];
    vec2 vBL = s_vel[lx - 1][ly - 1];
    vec2 vBR = s_vel[lx + 1][ly - 1];

    float curlL = abs((currentV.y - vL.y)       - (vTL.x - vBL.x) * 0.5);
    float curlR = abs((vR.y - currentV.y)       - (vTR.x - vBR.x) * 0.5);
    float curlB = abs((vBR.y - vBL.y) * 0.5     - (currentV.x - vB.x));
    float curlT = abs((vTR.y - vTL.y) * 0.5     - (vT.x - currentV.x));

    vec2 grad = vec2(curlR - curlL, curlT - curlB) * 0.5;
    float gradLen = length(grad);

    if (gradLen > 0.00001)
    {
        vec2 N = grad / (gradLen + 0.001);
        vec2 force = vec2(N.y, -N.x) * (curlCenter * push.vorticity); 
        advV += force * dt;
    }

    if (push.isMouseDown != 0)
    {
        vec2 screenRes = vec2(
            push.windowWidth > 0 ? float(push.windowWidth) : 1920.0,
            push.windowHeight > 0 ? float(push.windowHeight) : 1080.0
        );
        vec2 pixelPos = uv * screenRes;
        vec2 m0 = vec2(push.prevMouseX, push.prevMouseY);
        vec2 m1 = vec2(push.mouseX, push.mouseY);
        vec2 mDelta = m1 - m0;

        float mouseSpeed = length(mDelta);
        float dist = distToSegment(pixelPos, m0, m1);
        float speedFactor = smoothstep(0.0, 4.0, mouseSpeed);

        vec3 dyeColor = (mouseSpeed < 1.5) ? vec3(0.0, 0.8, 1.0) : getVelocityColor(mDelta);

        float forceRadius = push.splatRadius;
        if (dist < forceRadius)
        {
            float forceInf = exp(-(dist * dist) / (forceRadius * forceRadius * 0.4 + 0.001));
            
            vec2 fwd = (mouseSpeed > 0.0001) ? (mDelta / mouseSpeed) : vec2(0.0);
            vec2 side = vec2(-fwd.y, fwd.x);
            vec2 toPixel = pixelPos - m0;
            float sideDist = dot(toPixel, side);
            float swirl = clamp(sideDist / max(forceRadius, 0.001), -1.0, 1.0);
            vec2 forceDir = fwd * 0.7 + side * (swirl * 1.3);

            vec2 mForceUV = forceDir * (mouseSpeed / screenRes.x);
            advV += mForceUV * push.splatForce * forceInf * speedFactor;
        }

        float colorRadius = forceRadius * 0.9;
        if (dist < colorRadius)
        {
            float colorInf = exp(-(dist * dist) / (colorRadius * colorRadius * 0.3 + 0.001));
            float mixIntensity = colorInf * 0.65 * (0.1 + 0.9 * speedFactor);
            advC = mix(advC, vec4(dyeColor, 1.0), mixIntensity);
        }
    }

    advV = clamp(advV, vec2(-200.0), vec2(200.0));
    
    imageStore(outVelocity, pos, vec4(advV * push.velocityDissipation, 0.0, 0.0));
    imageStore(outColor, pos, advC * push.densityDissipation);

    if ((pos.x & 1) == 0 && (pos.y & 1) == 0)
    {
        ivec2 halfPos = pos >> 1;
        vec2 centerUV = (vec2(pos) + 1.0) * invSim;

        vec2 sampleR = texture(inVelocity, centerUV + vec2( 1.5,  0.0) * invSim).xy;
        vec2 sampleL = texture(inVelocity, centerUV + vec2(-1.5,  0.0) * invSim).xy;
        vec2 sampleT = texture(inVelocity, centerUV + vec2( 0.0,  1.5) * invSim).xy;
        vec2 sampleB = texture(inVelocity, centerUV + vec2( 0.0, -1.5) * invSim).xy;

        float div = 0.5 * ((sampleR.x - sampleL.x) + (sampleT.y - sampleB.y));
        imageStore(outDivergence, halfPos, vec4(clamp(div, -30.0, 30.0), 0.0, 0.0, 0.0));
    }
}