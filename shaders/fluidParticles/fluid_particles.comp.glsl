#version 450

layout(local_size_x = 256) in;

layout(constant_id = 0) const uint WORKGROUP_SIZE = 256;
layout(constant_id = 1) const uint PARTICLE_COUNT = 0;

struct Particle
{
    float x;
    float y;
    float prevX;
    float prevY;
    float vx;
    float vy;
    uint color; 
};

struct FluidCell
{
    float vx;
    float vy;
    float pressure;
    float divergence;
    float r;
    float g;
    float b;
    float a;
};

layout(std430, set = 0, binding = 0) buffer ParticleBuffer
{
    Particle particles[];
};

layout(std430, set = 0, binding = 1) buffer FluidBuffer
{
    FluidCell fluidCells[];
};

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

FluidCell getFluidCell(int x, int y)
{
    int clampedX = clamp(x, 0, int(push.simWidth) - 1);
    int clampedY = clamp(y, 0, int(push.simHeight) - 1);
    return fluidCells[clampedY * int(push.simWidth) + clampedX];
}

vec2 sampleFluidVelocity(vec2 uv)
{
    vec2 pos = uv * vec2(float(push.simWidth), float(push.simHeight)) - 0.5;
    
    int maxW = int(push.simWidth) - 1;
    int maxH = int(push.simHeight) - 1;
    
    ivec2 i0 = clamp(ivec2(floor(pos)), ivec2(0), ivec2(maxW, maxH));
    ivec2 i1 = clamp(i0 + 1,            ivec2(0), ivec2(maxW, maxH));
    vec2 f = fract(pos);

    vec2 v00 = vec2(getFluidCell(i0.x, i0.y).vx, getFluidCell(i0.x, i0.y).vy);
    vec2 v10 = vec2(getFluidCell(i1.x, i0.y).vx, getFluidCell(i1.x, i0.y).vy);
    vec2 v01 = vec2(getFluidCell(i0.x, i1.y).vx, getFluidCell(i0.x, i1.y).vy);
    vec2 v11 = vec2(getFluidCell(i1.x, i1.y).vx, getFluidCell(i1.x, i1.y).vy);

    return mix(mix(v00, v10, f.x), mix(v01, v11, f.x), f.y);
}

void main()
{
    uint id = gl_GlobalInvocationID.x;
    if (id >= PARTICLE_COUNT)
        return;

    Particle p = particles[id];

    vec2 screenRes = vec2(
        push.windowWidth > 0 ? float(push.windowWidth) : 1920.0,
        push.windowHeight > 0 ? float(push.windowHeight) : 1080.0
    );
    vec2 simRes = vec2(float(push.simWidth), float(push.simHeight));

    vec2 pos = vec2(p.x, p.y);
    vec2 vel = vec2(p.vx, p.vy);

    vec2 uv = pos / screenRes;
    vec2 fluidVelocity = sampleFluidVelocity(uv);

    float dragStrength = 0.45;
    vel = mix(vel, fluidVelocity * 50.0, dragStrength * push.dt);

    if (push.isMouseDown != 0)
    {
        vec2 mousePos = vec2(push.mouseX, push.mouseY);
        vec2 delta = mousePos - pos;
        float dist = length(delta);

        float attractionRadius = 300.0;
        if (dist < attractionRadius)
        {
            float forceFactor = (1.0 - dist / attractionRadius);
            
            vec2 pullForce = normalize(delta) * (forceFactor * 12.0);
            vel += pullForce;

            vec2 rotateForce = vec2(-delta.y, delta.x) * (forceFactor * 0.08);
            vel += rotateForce;
        }
    }

    vel *= 0.985;
    float maxVel = 400.0;
    if (length(vel) > maxVel)
    {
        vel = normalize(vel) * maxVel;
    }

    p.prevX = pos.x;
    p.prevY = pos.y;
    p.vx = vel.x;
    p.vy = vel.y;
    pos += vel * push.dt;

    if (pos.x < 0.0 || pos.x > screenRes.x) { vel.x *= -0.5; pos.x = clamp(pos.x, 0.0, screenRes.x); }
    if (pos.y < 0.0 || pos.y > screenRes.y) { vel.y *= -0.5; pos.y = clamp(pos.y, 0.0, screenRes.y); }

    p.x = pos.x;
    p.y = pos.y;

    ivec2 gridPos = ivec2((pos / screenRes) * simRes);

    if (gridPos.x >= 0 && gridPos.x < int(push.simWidth) &&
        gridPos.y >= 0 && gridPos.y < int(push.simHeight))
    {
        int cellIdx = gridPos.y * int(push.simWidth) + gridPos.x;

        vec4 pColor = unpackUnorm4x8(p.color);

        float velocityDeposit = 0.015;
        float colorDeposit = 0.08;

        fluidCells[cellIdx].vx += (vel.x / screenRes.x) * velocityDeposit;
        fluidCells[cellIdx].vy += (vel.y / screenRes.y) * velocityDeposit;

        fluidCells[cellIdx].r = clamp(fluidCells[cellIdx].r + pColor.r * colorDeposit, 0.0, 1.0);
        fluidCells[cellIdx].g = clamp(fluidCells[cellIdx].g + pColor.g * colorDeposit, 0.0, 1.0);
        fluidCells[cellIdx].b = clamp(fluidCells[cellIdx].b + pColor.b * colorDeposit, 0.0, 1.0);
        fluidCells[cellIdx].a = clamp(fluidCells[cellIdx].a + pColor.a * colorDeposit, 0.0, 1.0);
    }

    particles[id] = p;
}