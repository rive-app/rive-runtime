/*
 * Copyright 2022 Rive
 */

// Common math utilities.

#define PI 3.141592653589793238

INLINE float2 unchecked_mix(float2 a, float2 b, float t) { return (b - a) * t + a; }

INLINE float atan2(float2 v)
{
    float bias = .0;
    if (abs(v.x) > abs(v.y))
    {
        v = float2(v.y, -v.x);
        bias = PI / 2.;
    }
    return atan(v.y, v.x) + bias;
}

INLINE half4 unmultiply(half4 color)
{
    if (color.a != .0)
        color.rgb *= 1.0 / color.a;
    return color;
}
