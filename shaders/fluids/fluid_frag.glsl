#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// Sprzętowy sampler 2D z wbudowanym bilinearnym filtrowaniem
layout(binding = 0) uniform sampler2D fluidTexture;

layout(push_constant) uniform Push
{
    float screenWidth;
    float screenHeight;
    uint simWidth;
    uint simHeight;
} push;

void main()
{
    // Dedykowany moduł TMU karty graficznej sam sprzętowo interpoluje kolor!
    vec4 fluid = texture(fluidTexture, inUV);
    
    // Twój oryginalny post-processing kolorów:
    vec3 sharpColor = smoothstep(0.01, 0.75, fluid.rgb);
    sharpColor = pow(sharpColor, vec3(0.85)) * 1.25;

    outColor = vec4(sharpColor, 1.0);
}