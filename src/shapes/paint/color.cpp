#include "rive/shapes/paint/color.hpp"
#include <stdio.h>

namespace rive
{
	unsigned int colorARGB(int a, int r, int g, int b)
	{
		return (((a & 0xff) << 24) | ((r & 0xff) << 16) | ((g & 0xff) << 8) |
		        ((b & 0xff) << 0)) &
		       0xFFFFFFFF;
	}

	unsigned int colorRed(unsigned int value)
	{
		return (0x00ff0000 & value) >> 16;
	}

	unsigned int colorGreen(unsigned int value)
	{
		return (0x0000ff00 & value) >> 8;
	}

	unsigned int colorBlue(unsigned int value)
	{
		return (0x000000ff & value) >> 0;
	}

	unsigned int colorAlpha(unsigned int value)
	{
		return (0xff000000 & value) >> 24;
	}

	float colorOpacity(unsigned int value)
	{
		return (float)colorAlpha(value) / 0xFF;
	}

	unsigned int colorWithAlpha(unsigned int value, unsigned int a)
	{
		return colorARGB(
		    a, colorRed(value), colorGreen(value), colorBlue(value));
	}

	unsigned int colorWithOpacity(unsigned int value, float opacity)
	{
		return colorWithAlpha(value, std::round(255.0 * opacity));
	}

	unsigned int colorModulateOpacity(unsigned int value, float opacity)
	{
		return colorWithAlpha(
		    value, std::round(255.0f * colorOpacity(value) * opacity));
	}

	static unsigned int lerp(unsigned int a, unsigned int b, float mix)
	{
		return a * (1.0f - mix) + b * mix;
	}

	unsigned int colorLerp(unsigned int from, unsigned int to, float mix)
	{
		return colorARGB(lerp(colorAlpha(from), colorAlpha(to), mix),
		                 lerp(colorRed(from), colorRed(to), mix),
		                 lerp(colorGreen(from), colorGreen(to), mix),
		                 lerp(colorBlue(from), colorBlue(to), mix));
	}
} // namespace rive