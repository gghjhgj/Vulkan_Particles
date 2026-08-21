#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D inPressure;
layout(rg32f, binding = 1) readonly uniform image2D inVelocity;
layout(rg32f, binding = 2) writeonly uniform image2D outVelocity;

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

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= int(push.simWidth) || pos.y >= int(push.simHeight)) return;

    vec2 simSize = vec2(float(push.simWidth), float(push.simHeight));
    vec2 uv = (vec2(pos) + 0.5) / simSize;
    vec2 dUV = 1.0 / simSize;

    float pR = texture(inPressure, uv + vec2(dUV.x, 0.0)).r;
    float pL = texture(inPressure, uv - vec2(dUV.x, 0.0)).r;
    float pT = texture(inPressure, uv + vec2(0.0, dUV.y)).r;
    float pB = texture(inPressure, uv - vec2(0.0, dUV.y)).r;

    vec2 gradP = vec2(pR - pL, pT - pB) * 0.5;

    vec2 currentV = imageLoad(inVelocity, pos).xy;
    vec2 newV = currentV - gradP;

    imageStore(outVelocity, pos, vec4(newV, 0.0, 0.0));
}