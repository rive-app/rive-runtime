#ifndef _RIVE_LAYOUT_ENUMS_HPP_
#define _RIVE_LAYOUT_ENUMS_HPP_

#include <stdint.h>

namespace rive
{
enum class LayoutAnimationStyle : uint8_t
{
    none,
    inherit,
    custom
};

enum class LayoutStyleInterpolation : uint8_t
{
    hold,
    linear,
    cubic,
    elastic
};

enum class LayoutAlignmentType : uint8_t
{
    topLeft,
    topCenter,
    topRight,
    centerLeft,
    center,
    centerRight,
    bottomLeft,
    bottomCenter,
    bottomRight,
    spaceBetweenStart,
    spaceBetweenCenter,
    spaceBetweenEnd
};

enum class LayoutDirection : uint8_t
{
    inherit,
    ltr,
    rtl
};

enum class LayoutScaleType : uint8_t
{
    fixed,
    fill,
    hug
};
} // namespace rive
#endif