#version 450

layout(location = 0) in vec3 particleColor;
layout(location = 1) in float trailFactor;

layout(location = 0) out vec4 outColor;

void main()
{
    float brightness = trailFactor;

    outColor = vec4(
        particleColor,
        brightness
    );
}