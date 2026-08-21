#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// Wejścia jako próbkowniki (Hardware Bilinear + Hardware Clamp)
layout(binding = 0) uniform sampler2D inVelocity;
layout(binding = 1) uniform sampler2D inColor;

// Wyjścia jako Storage Images
layout(rg32f, binding = 2) writeonly uniform image2D outVelocity;
layout(rgba32f, binding = 3) writeonly uniform image2D outColor;
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
    if (pos.x >= int(push.simWidth) || pos.y >= int(push.simHeight)) return;

    vec2 simSize = vec2(float(push.simWidth), float(push.simHeight));
    vec2 uv = (vec2(pos) + 0.5) / simSize;
    vec2 invSim = 1.0 / simSize;

    float dt = (push.dt > 0.0 && push.dt < 0.1) ? push.dt : 0.016;

    // Sprzętowy odczyt bezpośrednio ze współrzędnych całkowitych (texelFetch)
    vec2 currentV = texelFetch(inVelocity, pos, 0).xy;

    // SPRZĘTOWE PRÓBKOWANIE DWULINIOWE W JEDNEJ LINICZCE!
    vec2 traceUV = uv - (currentV * dt) * invSim;
    vec2 advV = texture(inVelocity, traceUV).xy;
    vec4 advC = texture(inColor, traceUV);

    // Vorticity (odczyt sąsiadów przez texelFetch ze sprzętowym cachem 2D)
    vec2 vL = texelFetch(inVelocity, clamp(pos + ivec2(-1, 0), ivec2(0), ivec2(pos.x, int(push.simHeight)-1)), 0).xy;
    vec2 vR = texelFetch(inVelocity, clamp(pos + ivec2(1, 0), ivec2(0), ivec2(int(push.simWidth)-1, int(push.simHeight)-1)), 0).xy;
    vec2 vB = texelFetch(inVelocity, clamp(pos + ivec2(0, -1), ivec2(0), ivec2(int(push.simWidth)-1, pos.y)), 0).xy;
    vec2 vT = texelFetch(inVelocity, clamp(pos + ivec2(0, 1), ivec2(0), ivec2(int(push.simWidth)-1, int(push.simHeight)-1)), 0).xy;

    float curlCenter = (vR.y - vL.y) - (vT.x - vB.x);

    vec2 vLL = texelFetch(inVelocity, clamp(pos + ivec2(-2, 0), ivec2(0), ivec2(int(push.simWidth)-1, int(push.simHeight)-1)), 0).xy;
    vec2 vRR = texelFetch(inVelocity, clamp(pos + ivec2(2, 0), ivec2(0), ivec2(int(push.simWidth)-1, int(push.simHeight)-1)), 0).xy;
    vec2 vBB = texelFetch(inVelocity, clamp(pos + ivec2(0, -2), ivec2(0), ivec2(int(push.simWidth)-1, int(push.simHeight)-1)), 0).xy;
    vec2 vTT = texelFetch(inVelocity, clamp(pos + ivec2(0, 2), ivec2(0), ivec2(int(push.simWidth)-1, int(push.simHeight)-1)), 0).xy;

    float curlL = abs((currentV.y - vLL.y) - (texelFetch(inVelocity, clamp(pos + ivec2(-1, 1), ivec2(0), ivec2(int(push.simWidth)-1, int(push.simHeight)-1)), 0).x - texelFetch(inVelocity, clamp(pos + ivec2(-1, -1), ivec2(0), ivec2(int(push.simWidth)-1, int(push.simHeight)-1)), 0).x));
    float curlR = abs((vRR.y - currentV.y) - (texelFetch(inVelocity, clamp(pos + ivec2(1, 1), ivec2(0), ivec2(int(push.simWidth)-1, int(push.simHeight)-1)), 0).x - texelFetch(inVelocity, clamp(pos + ivec2(1, -1), ivec2(0), ivec2(int(push.simWidth)-1, int(push.simHeight)-1)), 0).x));
    float curlB = abs((texelFetch(inVelocity, clamp(pos + ivec2(1, -1), ivec2(0), ivec2(int(push.simWidth)-1, int(push.simHeight)-1)), 0).y - texelFetch(inVelocity, clamp(pos + ivec2(-1, -1), ivec2(0), ivec2(int(push.simWidth)-1, int(push.simHeight)-1)), 0).y) - (currentV.x - vBB.x));
    float curlT = abs((texelFetch(inVelocity, clamp(pos + ivec2(1, 1), ivec2(0), ivec2(int(push.simWidth)-1, int(push.simHeight)-1)), 0).y - texelFetch(inVelocity, clamp(pos + ivec2(-1, 1), ivec2(0), ivec2(int(push.simWidth)-1, int(push.simHeight)-1)), 0).y) - (vTT.x - currentV.x));

    vec2 grad = vec2(curlR - curlL, curlT - curlB) * 0.5;
    float gradLen = length(grad);

    if (gradLen > 0.00001)
    {
        vec2 N = grad / (gradLen + 0.001);
        vec2 force = vec2(N.y, -N.x) * curlCenter * push.vorticity; 
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
            vec2 mDeltaUV = mDelta / screenRes;
            advV += mDeltaUV * push.splatForce * forceInf * speedFactor;
        }

        float colorRadius = forceRadius * 0.9;
        if (dist < colorRadius)
        {
            float colorInf = exp(-(dist * dist) / (colorRadius * colorRadius * 0.3 + 0.001));
            float mixIntensity = colorInf * 0.65 * (0.1 + 0.9 * speedFactor);
            advC = mix(advC, vec4(dyeColor, 1.0), mixIntensity);
        }
    }

    advV = clamp(advV, vec2(-100.0), vec2(100.0));
    
    // Zapis do Storage Images
    imageStore(outVelocity, pos, vec4(advV * push.velocityDissipation, 0.0, 0.0));
    imageStore(outColor, pos, advC * push.densityDissipation);

    float div = 0.5 * ((vR.x - vL.x) + (vT.y - vB.y));
    imageStore(outDivergence, pos, vec4(clamp(div, -30.0, 30.0), 0.0, 0.0, 0.0));
}