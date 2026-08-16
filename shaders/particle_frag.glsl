#version 450

layout(location = 0) flat in vec3 particleColor;
layout(location = 1) in vec2 localPos;
layout(location = 2) flat in vec4 trailData;

layout(location = 0) out vec4 outColor;

void main()
{
    float fade = clamp((localPos.x + trailData.x) * trailData.z, 0.0, 1.0);
    float taper = fade * fade * fade;
    float radius = localPos.x > 0.0
        ? 1.0
        : (taper * 0.65 + 0.35);
    float shape = clamp(
        (radius * trailData.y - abs(localPos.y)) * trailData.w,
        0.0,
        1.0
    );
    float alpha = shape * fade * fade * (fade * 0.45 + 0.55);
    outColor = vec4(particleColor, alpha);
}