#ifndef MATH_GLSL
#define MATH_GLSL

float random(float seed)
{
    return fract(sin(seed) * 43758.5453123);
}

float distToSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a;
    vec2 ba = b - a;
    float d = dot(ba, ba);
    float h = (d > 0.00001) ? clamp(dot(pa, ba) / d, 0.0, 1.0) : 0.0;
    return length(pa - ba * h);
}

vec2 applySoftSpeedLimitFast(vec2 v, float threshold, float hardMax)
{
    float speedSq = dot(v, v);
    if (speedSq > threshold * threshold)
    {
        float speed = sqrt(speedSq);
        float excess = speed - threshold;
        float maxExcess = hardMax - threshold;
        float compressed = threshold + maxExcess * (excess / (excess + maxExcess));
        return (v / speed) * compressed;
    }
    return v;
}

#endif