#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct FluidCell
{
    float vx;
    float vy;
    float pressure;
    float divergence;
    vec4 color; 
};

layout(std430, binding = 0) readonly buffer InBuffer  { FluidCell inCells[]; };
layout(std430, binding = 1) writeonly buffer OutBuffer { FluidCell outCells[]; };

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

vec4 sampleColor(vec2 uv)
{
    vec2 pos = uv * vec2(float(push.simWidth), float(push.simHeight)) - 0.5;
    
    int maxW = int(push.simWidth) - 1;
    int maxH = int(push.simHeight) - 1;
    
    ivec2 i0 = clamp(ivec2(floor(pos)), ivec2(0), ivec2(maxW, maxH));
    ivec2 i1 = clamp(i0 + 1,            ivec2(0), ivec2(maxW, maxH));
    vec2 f = fract(pos);

    vec4 c00 = inCells[i0.y * int(push.simWidth) + i0.x].color;
    vec4 c10 = inCells[i0.y * int(push.simWidth) + i1.x].color;
    vec4 c01 = inCells[i1.y * int(push.simWidth) + i0.x].color;
    vec4 c11 = inCells[i1.y * int(push.simWidth) + i1.x].color;

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

    vec2 v00 = vec2(inCells[i0.y * int(push.simWidth) + i0.x].vx, inCells[i0.y * int(push.simWidth) + i0.x].vy);
    vec2 v10 = vec2(inCells[i0.y * int(push.simWidth) + i1.x].vx, inCells[i0.y * int(push.simWidth) + i1.x].vy);
    vec2 v01 = vec2(inCells[i1.y * int(push.simWidth) + i0.x].vx, inCells[i1.y * int(push.simWidth) + i0.x].vy);
    vec2 v11 = vec2(inCells[i1.y * int(push.simWidth) + i1.x].vx, inCells[i1.y * int(push.simWidth) + i1.x].vy);

    return mix(mix(v00, v10, f.x), mix(v01, v11, f.x), f.y);
}

float distToSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a;
    vec2 ba = b - a;
    float d = dot(ba, ba);
    float h = (d > 0.00001) ? clamp(dot(pa, ba) / d, 0.0, 1.0) : 0.0;
    return length(pa - ba * h);
}

FluidCell getCell(int x, int y)
{
    int maxW = int(push.simWidth) - 1;
    int maxH = int(push.simHeight) - 1;
    
    x = clamp(x, 0, maxW);
    y = clamp(y, 0, maxH);
    return inCells[y * int(push.simWidth) + x];
}

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= push.simWidth * push.simHeight) return;

    int x = int(id % push.simWidth);
    int y = int(id / push.simWidth);

    vec2 simSize = vec2(float(push.simWidth), float(push.simHeight));
    vec2 uv = (vec2(float(x), float(y)) + 0.5) / simSize;

    FluidCell currentCell = inCells[id];
    vec2 currentV = vec2(currentCell.vx, currentCell.vy);

    float dt = (push.dt > 0.0 && push.dt < 0.1) ? push.dt : 0.016;

    vec2 traceUV = uv - (currentV * dt) / simSize;
    vec2 advV = sampleVelocity(traceUV);
    vec4 advC = sampleColor(traceUV);

    float curlCenter = (getCell(x + 1, y).vy - getCell(x - 1, y).vy) - (getCell(x, y + 1).vx - getCell(x, y - 1).vx);
    float curlL = abs((getCell(x, y).vy - getCell(x - 2, y).vy) - (getCell(x - 1, y + 1).vx - getCell(x - 1, y - 1).vx));
    float curlR = abs((getCell(x + 2, y).vy - getCell(x, y).vy) - (getCell(x + 1, y + 1).vx - getCell(x + 1, y - 1).vx));
    float curlB = abs((getCell(x + 1, y - 1).vy - getCell(x - 1, y - 1).vy) - (getCell(x, y).vx - getCell(x, y - 2).vx));
    float curlT = abs((getCell(x + 1, y + 1).vy - getCell(x - 1, y + 1).vy) - (getCell(x, y + 2).vx - getCell(x, y).vx));

    vec2 grad = vec2(curlR - curlL, curlT - curlB) * 0.5;
    float gradLen = length(grad);

    if (gradLen > 0.00001)
    {
        vec2 N = grad / (gradLen + 0.001);
        
        float vorticityStrength = push.vorticity; 
        
        vec2 force = vec2(N.y, -N.x) * curlCenter * vorticityStrength; 
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

        vec3 dyeColor;
        if (mouseSpeed < 1.5)
        {
            dyeColor = vec3(0.0, 0.8, 1.0); 
        }
        else
        {
            dyeColor = getVelocityColor(mDelta); 
        }

        float forceRadius = push.splatRadius;
        if (dist < forceRadius)
        {
            float forceInf = exp(-(dist * dist) / (forceRadius * forceRadius * 0.4 + 0.001));
            
            float force = push.splatForce;
            
            vec2 mDeltaUV = mDelta / screenRes;
            vec2 vFromMouse = mDeltaUV * force;
            
            advV += vFromMouse * forceInf * speedFactor;
        }

        float colorRadius = forceRadius * 0.9;
        if (dist < colorRadius)
        {
            float colorInf = exp(-(dist * dist) / (colorRadius * colorRadius * 0.3 + 0.001));
            float mixIntensity = colorInf * 0.65 * (0.1 + 0.9 * speedFactor);
            advC = mix(advC, vec4(dyeColor, 1.0), mixIntensity);
        }
    }

    float uLeft  = getCell(x - 1, y).vx;
    float uRight = getCell(x + 1, y).vx;
    float vDown  = getCell(x, y - 1).vy;
    float vUp    = getCell(x, y + 1).vy;
    float div = 0.5 * ((uRight - uLeft) + (vUp - vDown));

    float vDiss = push.velocityDissipation;
    float dDiss = push.densityDissipation;

    advV = clamp(advV, vec2(-100.0), vec2(100.0));

    FluidCell outC;
    outC.vx = advV.x * vDiss;
    outC.vy = advV.y * vDiss;
    outC.pressure = 0.0; 
    outC.divergence = clamp(div, -30.0, 30.0);

    outC.color = advC * dDiss;

    outCells[id] = outC;
}