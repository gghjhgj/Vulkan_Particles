#ifndef COLOR_GLSL
#define COLOR_GLSL

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 getVelocityColor(vec2 dir)
{
    float len = length(dir);
    if (len < 0.001) return vec3(0.0, 0.8, 1.0);
    float angle = atan(dir.y, dir.x);
    return 0.5 + 0.5 * cos(angle + vec3(0.0, 2.0, 4.0));
}

#endif