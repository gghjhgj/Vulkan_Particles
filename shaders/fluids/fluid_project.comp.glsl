#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D inPressure;
layout(rg16f, binding = 1) readonly uniform image2D inVelocity;
layout(rg16f, binding = 2) writeonly uniform image2D outVelocity;

layout(push_constant) uniform Push
{
    float mouseX, mouseY, prevMouseX, prevMouseY;
    float dt, splatRadius, splatForce;
    float velocityDissipation, densityDissipation, vorticity;        
    uint simWidth, simHeight, windowWidth, windowHeight;      
    uint isMouseDown, offsetFromLeft, offsetFromRight;
} push;

void main()
{
    uint leftBound = push.offsetFromLeft;
    uint rightBound = (push.simWidth > push.offsetFromRight) ? (push.simWidth - push.offsetFromRight) : 0;
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    if (pos.x < leftBound || pos.x >= rightBound || pos.x >= push.simWidth || pos.y >= push.simHeight) 
        return;

    vec2 invSim = 1.0 / vec2(float(push.simWidth), float(push.simHeight));
    vec2 uv = (vec2(pos) + 0.5) * invSim;

    float pCenter = texture(inPressure, uv).r;
    float pR = (pos.x < int(rightBound) - 1)     ? textureOffset(inPressure, uv, ivec2( 1,  0)).r : pCenter;
    float pL = (pos.x > int(leftBound))          ? textureOffset(inPressure, uv, ivec2(-1,  0)).r : pCenter;
    float pT = (pos.y < int(push.simHeight) - 1) ? textureOffset(inPressure, uv, ivec2( 0,  1)).r : pCenter;
    float pB = (pos.y > 0)                       ? textureOffset(inPressure, uv, ivec2( 0, -1)).r : pCenter;

    float scaleX = (pos.x > int(leftBound) && pos.x < int(rightBound) - 1) ? 0.5 : 1.0;
    float scaleY = (pos.y > 0 && pos.y < int(push.simHeight) - 1) ? 0.5 : 1.0;

    vec2 gradP = vec2((pR - pL) * scaleX, (pT - pB) * scaleY);

    const float MAX_PRESSURE_KICK = 500.0;
    float gradLenSq = dot(gradP, gradP);
    if (gradLenSq > MAX_PRESSURE_KICK * MAX_PRESSURE_KICK)
    {
        gradP *= (MAX_PRESSURE_KICK * inversesqrt(gradLenSq));
    }

    vec2 newV = imageLoad(inVelocity, pos).xy - gradP;

    if (pos.x <= int(leftBound))          newV.x = max(0.0, newV.x);
    if (pos.x >= int(rightBound) - 1)     newV.x = min(0.0, newV.x);
    if (pos.y <= 0)                       newV.y = max(0.0, newV.y);
    if (pos.y >= int(push.simHeight) - 1) newV.y = min(0.0, newV.y);

    imageStore(outVelocity, pos, vec4(newV, 0.0, 0.0));
}