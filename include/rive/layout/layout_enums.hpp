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

// Distribution along a container's own main axis.
enum class LayoutMainDistribute : uint8_t
{
    start,
    center,
    end,
    spaceBetween
};

// Placement along a container's own cross axis.
enum class LayoutCrossAlign : uint8_t
{
    start,
    center,
    end
};

struct LayoutContainerAlignment
{
    LayoutMainDistribute main;
    LayoutCrossAlign cross;
};

// The authored alignment on a container's own axes. The 9 positions name
// screen directions, so they swap with the main axis; space-between always
// distributes the main axis.
inline LayoutContainerAlignment containerAlignment(LayoutAlignmentType type,
                                                   bool isRow)
{
    LayoutMainDistribute horizontal;
    LayoutCrossAlign vertical;
    switch (type)
    {
        case LayoutAlignmentType::spaceBetweenStart:
            return {LayoutMainDistribute::spaceBetween,
                    LayoutCrossAlign::start};
        case LayoutAlignmentType::spaceBetweenCenter:
            return {LayoutMainDistribute::spaceBetween,
                    LayoutCrossAlign::center};
        case LayoutAlignmentType::spaceBetweenEnd:
            return {LayoutMainDistribute::spaceBetween, LayoutCrossAlign::end};
        case LayoutAlignmentType::topLeft:
        case LayoutAlignmentType::centerLeft:
        case LayoutAlignmentType::bottomLeft:
            horizontal = LayoutMainDistribute::start;
            break;
        case LayoutAlignmentType::topCenter:
        case LayoutAlignmentType::center:
        case LayoutAlignmentType::bottomCenter:
            horizontal = LayoutMainDistribute::center;
            break;
        default:
            horizontal = LayoutMainDistribute::end;
            break;
    }
    switch (type)
    {
        case LayoutAlignmentType::topLeft:
        case LayoutAlignmentType::topCenter:
        case LayoutAlignmentType::topRight:
            vertical = LayoutCrossAlign::start;
            break;
        case LayoutAlignmentType::centerLeft:
        case LayoutAlignmentType::center:
        case LayoutAlignmentType::centerRight:
            vertical = LayoutCrossAlign::center;
            break;
        default:
            vertical = LayoutCrossAlign::end;
            break;
    }
    if (isRow)
    {
        return {horizontal, vertical};
    }
    // A column's main axis is vertical: the vertical intent distributes it.
    return {LayoutMainDistribute(vertical), LayoutCrossAlign(horizontal)};
}

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