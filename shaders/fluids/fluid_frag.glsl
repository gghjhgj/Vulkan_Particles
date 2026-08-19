#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

struct FluidCell
{
    float vx;
    float vy;
    float pressure;
    float divergence;
    vec4 color;
};

layout(std430, binding = 0) readonly buffer FluidBuffer
{
    FluidCell cells[];
};

layout(push_constant) uniform Push
{
    float screenWidth;
    float screenHeight;
    uint simWidth;
    uint simHeight;
} push;

vec4 sampleFluid(vec2 uv)
{
    vec2 pos = uv * vec2(float(push.simWidth), float(push.simHeight)) - 0.5;
    
    int maxW = int(push.simWidth) - 1;
    int maxH = int(push.simHeight) - 1;

    ivec2 i0 = clamp(ivec2(floor(pos)), ivec2(0), ivec2(maxW, maxH));
    ivec2 i1 = clamp(i0 + 1,             ivec2(0), ivec2(maxW, maxH));
    vec2 f = fract(pos);

    vec4 c00 = cells[i0.y * int(push.simWidth) + i0.x].color;
    vec4 c10 = cells[i0.y * int(push.simWidth) + i1.x].color;
    vec4 c01 = cells[i1.y * int(push.simWidth) + i0.x].color;
    vec4 c11 = cells[i1.y * int(push.simWidth) + i1.x].color;

    return mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
}

void main()
{
    vec4 fluid = sampleFluid(inUV);
    
    vec3 sharpColor = smoothstep(0.01, 0.75, fluid.rgb);
    sharpColor = pow(sharpColor, vec3(0.85)) * 1.25;

    outColor = vec4(sharpColor, 1.0);
}