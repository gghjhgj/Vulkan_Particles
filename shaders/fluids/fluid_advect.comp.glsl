#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(std430, binding = 0) readonly buffer  VelIn  { vec2 inVelocity[]; };
layout(std430, binding = 1) writeonly buffer VelOut { vec2 outVelocity[]; };
layout(std430, binding = 2) readonly buffer  ColIn  { vec4 inColor[]; };
layout(std430, binding = 3) writeonly buffer ColOut { vec4 outColor[]; };
layout(std430, binding = 4) writeonly buffer DivOut { float outDivergence[]; };

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

vec2 getVel(int x, int y)
{
    x = clamp(x, 0, int(push.simWidth - 1));
    y = clamp(y, 0, int(push.simHeight - 1));
    return inVelocity[y * int(push.simWidth) + x];
}

vec4 sampleColor(vec2 uv)
{
    vec2 pos = uv * vec2(float(push.simWidth), float(push.simHeight)) - 0.5;
    
    int maxW = int(push.simWidth) - 1;
    int maxH = int(push.simHeight) - 1;
    
    ivec2 i0 = clamp(ivec2(floor(pos)), ivec2(0), ivec2(maxW, maxH));
    ivec2 i1 = clamp(i0 + 1,            ivec2(0), ivec2(maxW, maxH));
    vec2 f = fract(pos);

    vec4 c00 = inColor[i0.y * int(push.simWidth) + i0.x];
    vec4 c10 = inColor[i0.y * int(push.simWidth) + i1.x];
    vec4 c01 = inColor[i1.y * int(push.simWidth) + i0.x];
    vec4 c11 = inColor[i1.y * int(push.simWidth) + i1.x];

    return mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
}

vec2 sampleVelocity(vec2 uv)
{
    vec2 pos = uv * vec2(float(push.simWidth), float(push.simHeight)) - 0.5;
    
    int maxW = int(push.simWidth) - 1;
    int maxH = int(push.simHeight) - 1;
    
    ivec2 i0 = clamp(ivec2(floor(pos)), ivec2(0), ivec2(maxW, maxH));
    ivec2 i1 = clamp(i0 + 1,            ivec2(0), ivec2(maxW, maxH));
    vec2 f = fract(pos);

    vec2 v00 = inVelocity[i0.y * int(push.simWidth) + i0.x];
    vec2 v10 = inVelocity[i0.y * int(push.simWidth) + i1.x];
    vec2 v01 = inVelocity[i1.y * int(push.simWidth) + i0.x];
    vec2 v11 = inVelocity[i1.y * int(push.simWidth) + i1.x];

    return mix(mix(v00, v10, f.x), mix(v01, v11, f.x), f.y);
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
    if (pos.x >= int(push.simWidth) || pos.y >= int(push.simHeight)) return;

    int x = pos.x;
    int y = pos.y;
    uint id = uint(y) * push.simWidth + uint(x);

    vec2 simSize = vec2(float(push.simWidth), float(push.simHeight));
    vec2 uv = (vec2(float(x), float(y)) + 0.5) / simSize;

    vec2 currentV = inVelocity[id];
    float dt = (push.dt > 0.0 && push.dt < 0.1) ? push.dt : 0.016;

    vec2 traceUV = uv - (currentV * dt) / simSize;
    vec2 advV = sampleVelocity(traceUV);
    vec4 advC = sampleColor(traceUV);

    float curlCenter = (getVel(x + 1, y).y - getVel(x - 1, y).y) - (getVel(x, y + 1).x - getVel(x, y - 1).x);
    float curlL = abs((getVel(x, y).y - getVel(x - 2, y).y) - (getVel(x - 1, y + 1).x - getVel(x - 1, y - 1).x));
    float curlR = abs((getVel(x + 2, y).y - getVel(x, y).y) - (getVel(x + 1, y + 1).x - getVel(x + 1, y - 1).x));
    float curlB = abs((getVel(x + 1, y - 1).y - getVel(x - 1, y - 1).y) - (getVel(x, y).x - getVel(x, y - 2).x));
    float curlT = abs((getVel(x + 1, y + 1).y - getVel(x - 1, y + 1).y) - (getVel(x, y + 2).x - getVel(x, y).x));

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
    
    outVelocity[id] = advV * push.velocityDissipation;
    outColor[id] = advC * push.densityDissipation;

    float uLeft  = getVel(x - 1, y).x;
    float uRight = getVel(x + 1, y).x;
    float vDown  = getVel(x, y - 1).y;
    float vUp    = getVel(x, y + 1).y;
    
    float div = 0.5 * ((uRight - uLeft) + (vUp - vDown));
    outDivergence[id] = clamp(div, -30.0, 30.0);
}