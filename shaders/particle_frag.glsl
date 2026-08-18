#version 450

layout(location = 0) flat in vec3 particleColor;
layout(location = 1) in vec2 localPos;
layout(location = 2) flat in vec4 trailData;

layout(location = 0) out vec4 outColor;

void main()
{
    float fade = clamp((localPos.x + trailData.x) * trailData.z, 0.0, 1.0);
    float taper = fade * fade * fade;

    float t = clamp(-localPos.x / max(trailData.x, 0.001), 0.0, 1.0);
    vec2 proj = vec2(-trailData.x * t, 0.0);

    float radius = localPos.x > 0.0 ? 1.0 : (taper * 0.65 + 0.35);

    float distToSegment = length(vec2(localPos.x - proj.x, localPos.y / radius));

    float shape = clamp((trailData.y - distToSegment * trailData.y) * trailData.w, 0.0, 1.0);

    float alpha = shape * fade * fade * (fade * 0.45 + 0.55);
    outColor = vec4(particleColor, alpha);
}