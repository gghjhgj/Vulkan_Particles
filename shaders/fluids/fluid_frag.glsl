#version 450 
 
layout(location = 0) in vec2 inUV; 
layout(location = 0) out vec4 outColor; 
 
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
    vec2 texel = 1.0 / vec2(float(push.simWidth), float(push.simHeight)); 
 
    vec4 colC = texture(fluidTexture, inUV); 
    vec4 colL = texture(fluidTexture, inUV - vec2(texel.x, 0.0)); 
    vec4 colR = texture(fluidTexture, inUV + vec2(texel.x, 0.0)); 
    vec4 colB = texture(fluidTexture, inUV - vec2(0.0, texel.y)); 
    vec4 colT = texture(fluidTexture, inUV + vec2(0.0, texel.y)); 

    vec4 avgNeighbors = (colL + colR + colB + colT) * 0.25; 
    vec4 highPassDelta = colC - avgNeighbors; 
 
    const float SHARPNESS = 0.75; 
    vec3 sharpened = colC.rgb + highPassDelta.rgb * SHARPNESS; 

    float densL = length(colL.rgb); 
    float densR = length(colR.rgb); 
    float densB = length(colB.rgb); 
    float densT = length(colT.rgb); 
 
    vec2 grad = vec2(densR - densL, densT - densB); 
     
    vec3 normal = normalize(vec3(-grad * 1.5, 1.0)); 
    vec3 lightDir = normalize(vec3(0.5, 0.8, 1.0)); 
 
    float diffuse = clamp(dot(normal, lightDir), 0.75, 1.15); 
    sharpened *= diffuse; 
    
    sharpened = smoothstep(0.01, 0.95, sharpened); 
 
    sharpened = pow(sharpened, vec3(0.95)); 
 
    sharpened *= 1.15; 
 
    outColor = vec4(clamp(sharpened, 0.0, 1.0), 1.0); 
}