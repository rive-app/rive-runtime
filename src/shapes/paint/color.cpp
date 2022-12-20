#include "rive/shapes/paint/color.hpp"

#include "rive/math/simd.hpp"
#include <stdio.h>

namespace rive
{
unsigned int colorARGB(int a, int r, int g, int b)
{
    return (((a & 0xff) << 24) | ((r & 0xff) << 16) | ((g & 0xff) << 8) | ((b & 0xff) << 0)) &
           0xFFFFFFFF;
}

unsigned int colorRed(ColorInt value) { return (0x00ff0000 & value) >> 16; }

unsigned int colorGreen(ColorInt value) { return (0x0000ff00 & value) >> 8; }

unsigned int colorBlue(ColorInt value) { return (0x000000ff & value) >> 0; }

unsigned int colorAlpha(ColorInt value) { return (0xff000000 & value) >> 24; }

void UnpackColor4f(ColorInt color, float out[4])
{
    float4 color4f = simd::cast<float>(color << uint4{8, 16, 24, 0} >> 24u) / 255.f;
    simd::store(out, color4f);
}

float colorOpacity(ColorInt value) { return (float)colorAlpha(value) / 0xFF; }

ColorInt colorWithAlpha(ColorInt value, unsigned int a)
{
    return colorARGB(a, colorRed(value), colorGreen(value), colorBlue(value));
}

ColorInt colorWithOpacity(ColorInt value, float opacity)
{
    return colorWithAlpha(value, std::lround(255.f * opacity));
}

ColorInt colorModulateOpacity(ColorInt value, float opacity)
{
    return colorWithAlpha(value, std::lround(255.f * colorOpacity(value) * opacity));
}

static unsigned int lerp(unsigned int a, unsigned int b, float mix)
{
    return std::lround(a * (1.0f - mix) + b * mix);
}

ColorInt colorLerp(ColorInt from, ColorInt to, float mix)
{
    return colorARGB(lerp(colorAlpha(from), colorAlpha(to), mix),
                     lerp(colorRed(from), colorRed(to), mix),
                     lerp(colorGreen(from), colorGreen(to), mix),
                     lerp(colorBlue(from), colorBlue(to), mix));
}
} // namespace rive
