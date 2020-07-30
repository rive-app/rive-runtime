#ifndef _RIVE_PAINT_COLOR_HPP_
#define _RIVE_PAINT_COLOR_HPP_
#include <cmath>

namespace rive
{
	unsigned int colorARGB(int a, int r, int g, int b);

	unsigned int colorRed(unsigned int value);

	unsigned int colorGreen(unsigned int value);

	unsigned int colorBlue(unsigned int value);

	unsigned int colorAlpha(unsigned int value);

	float colorOpacity(unsigned int value);

	unsigned int colorWithAlpha(unsigned int value, unsigned int a);

	unsigned int colorWithOpacity(unsigned int value, float opacity);

	unsigned int colorModulateOpacity(unsigned int value, float opacity);

	unsigned int colorLerp(unsigned int from, unsigned int to, float mix);
} // namespace rive
#endif
