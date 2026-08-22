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
    
    float sharpness;
    float highPassLimit;
    float normalStrength;
    float lightDirX;
    float lightDirY;
    float lightDirZ;
    float lightIntensity;
    float ambientLight;
    float colorBoost;
    float gamma;
    float exposure;
    float blurStrength;
} push; 
 
void main() 
{ 
    vec2 simTexel = 1.0 / vec2(float(push.simWidth), float(push.simHeight)); 
 
    vec2 smoothUV = inUV + simTexel * 0.5;

    vec4 colC = texture(fluidTexture, smoothUV); 
    vec4 colL = texture(fluidTexture, smoothUV - vec2(simTexel.x, 0.0)); 
    vec4 colR = texture(fluidTexture, smoothUV + vec2(simTexel.x, 0.0)); 
    vec4 colB = texture(fluidTexture, smoothUV - vec2(0.0, simTexel.y)); 
    vec4 colT = texture(fluidTexture, smoothUV + vec2(0.0, simTexel.y)); 

    vec4 avgNeighbors = (colL + colR + colB + colT) * 0.25; 
    
    vec4 highPassDelta = colC - avgNeighbors; 
 
    float safeBlur = clamp(push.blurStrength, 0.0, 1.0); 
    
    vec3 baseColor = mix(colC.rgb, avgNeighbors.rgb, safeBlur);

    vec3 highPass = highPassDelta.rgb * push.sharpness;
    highPass = max(highPass, -baseColor * push.highPassLimit);
    vec3 color = max(baseColor + highPass, 0.0); 

    float densL = length(colL.rgb); 
    float densR = length(colR.rgb); 
    float densB = length(colB.rgb); 
    float densT = length(colT.rgb); 
 
    vec2 grad = vec2(densR - densL, densT - densB); 
    vec3 normal = normalize(vec3(-grad * push.normalStrength, 1.0)); 
    vec3 lightDir = normalize(vec3(push.lightDirX, push.lightDirY, push.lightDirZ)); 

    float light = clamp(dot(normal, lightDir) * push.lightIntensity + push.ambientLight, 0.90, 1.08); 
    color *= light; 

    float densC = length(color);
    color += color * (smoothstep(0.3, 0.8, densC) * push.colorBoost);

    color = smoothstep(0.0, 0.98, color); 
    color = pow(max(color, 0.0), vec3(push.gamma)) * push.exposure; 
 
    outColor = vec4(clamp(color, 0.0, 1.0), 1.0); 
}