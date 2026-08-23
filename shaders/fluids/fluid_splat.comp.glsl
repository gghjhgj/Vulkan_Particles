#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(rg16f, binding = 0) uniform image2D velocityImage;
layout(rgba8, binding = 1) uniform image2D colorImage;

layout(push_constant) uniform Push
{
    float mouseX, mouseY, prevMouseX, prevMouseY;
    float dt, splatRadius, splatForce;
    float velocityDissipation, densityDissipation, vorticity;
    uint renderWidth, renderHeight;
    uint simWidth, simHeight;
    uint pressWidth, pressHeight;
    uint windowWidth, windowHeight;
    uint isMouseDown;
    uint offsetFromLeft, offsetFromRight, offsetFromUp, offsetFromDown;
    float omega;
    uint pressureSteps;
} push;

float distToSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a;
    vec2 ba = b - a;
    float d = dot(ba, ba);
    float h = (d > 0.00001) ? clamp(dot(pa, ba) / d, 0.0, 1.0) : 0.0;
    return length(pa - ba * h);
}

vec3 getVelocityColor(vec2 dir)
{
    float len = length(dir);
    if (len < 0.001) return vec3(0.0, 0.8, 1.0);
    float angle = atan(dir.y, dir.x);
    return 0.5 + 0.5 * cos(angle + vec3(0.0, 2.0, 4.0));
}

void main()
{
    if (push.isMouseDown == 0) return;

    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= int(push.simWidth) || pos.y >= int(push.simHeight)) return;

    float scaleX = float(push.simWidth) / float(push.windowWidth);
    float scaleY = float(push.simHeight) / float(push.windowHeight);

    uint leftBound = uint(float(push.offsetFromLeft) * scaleX);
    uint rightOffset = uint(float(push.offsetFromRight) * scaleX);
    uint rightBound = (push.simWidth > rightOffset) ? (push.simWidth - rightOffset) : 0;

    uint upBound = uint(float(push.offsetFromUp) * scaleY);
    uint downOffset = uint(float(push.offsetFromDown) * scaleY);
    uint downBound = (push.simHeight > downOffset) ? (push.simHeight - downOffset) : 0;

    if (pos.x < int(leftBound) || pos.x >= int(rightBound) || pos.y < int(upBound) || pos.y >= int(downBound)) return;

    vec2 screenRes = vec2(
        push.windowWidth > 0 ? float(push.windowWidth) : 1920.0,
        push.windowHeight > 0 ? float(push.windowHeight) : 1080.0
    );

    vec2 simSize = vec2(float(push.simWidth), float(push.simHeight));
    vec2 uv = (vec2(pos) + 0.5) / simSize;
    vec2 pixelPos = uv * screenRes;

    vec2 m0 = vec2(push.prevMouseX, push.prevMouseY);
    vec2 m1 = vec2(push.mouseX, push.mouseY);
    vec2 mDelta = m1 - m0;

    float dist = distToSegment(pixelPos, m0, m1);
    float forceRadius = push.splatRadius;

    if (dist > forceRadius) return;

    vec2 currentV = imageLoad(velocityImage, pos).xy;
    vec4 currentC = imageLoad(colorImage, pos);

    float mouseSpeed = length(mDelta);
    float speedFactor = (mouseSpeed < 0.001) ? 1.0 : smoothstep(0.0, 4.0, mouseSpeed);

    float forceInf = exp(-(dist * dist) / (forceRadius * forceRadius * 0.4 + 0.001));
    vec2 fwd = (mouseSpeed > 0.0001) ? (mDelta / mouseSpeed) : normalize(pixelPos - m0 + vec2(0.0001));
    vec2 side = vec2(-fwd.y, fwd.x);
    vec2 toPixel = pixelPos - m0;
    float sideDist = dot(toPixel, side);
    float swirl = clamp(sideDist / max(forceRadius, 0.001), -1.0, 1.0);
    vec2 forceDir = fwd * 0.7 + side * (swirl * 1.3);

    vec2 mForceUV = forceDir * (max(mouseSpeed, 2.0) / screenRes.x);
    float simScaleFactor = simSize.x / screenRes.x;
    currentV += mForceUV * push.splatForce * simScaleFactor * forceInf * speedFactor;

    currentV = clamp(currentV, vec2(-60000.0), vec2(60000.0));

    float colorRadius = forceRadius * 0.9;
    if (dist < colorRadius)
    {
        vec3 dyeColor = (mouseSpeed < 1.5) ? vec3(0.0, 0.8, 1.0) : getVelocityColor(mDelta);
        float colorInf = exp(-(dist * dist) / (colorRadius * colorRadius * 0.3 + 0.001));
        float mixIntensity = colorInf * 0.65 * (0.2 + 0.8 * speedFactor);
        currentC = mix(currentC, vec4(dyeColor, 1.0), mixIntensity);
    }

    imageStore(velocityImage, pos, vec4(currentV, 0.0, 0.0));
    imageStore(colorImage, pos, currentC);
}