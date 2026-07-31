#include "rive/layout/layout_sizing_style.hpp"
#include "rive/layout/grid_track.hpp"
#include "rive/layout/layout_enums.hpp"
#ifdef WITH_RIVE_LAYOUT
// YGStyle is only forward declared in the headers; the setters below need it.
#include "rive/layout/layout_data.hpp"
#endif

using namespace rive;

#ifdef WITH_RIVE_LAYOUT
YGDisplay LayoutSizingStyle::display()
{
    return YGDisplay(displayValue()) == YGDisplayNone ? YGDisplayNone
                                                      : YGDisplayFlex;
}

YGUnit LayoutSizingStyle::maxWidthUnits()
{
    return YGUnit(maxWidthUnitsValue());
}

YGUnit LayoutSizingStyle::maxHeightUnits()
{
    return YGUnit(maxHeightUnitsValue());
}

YGUnit LayoutSizingStyle::minWidthUnits()
{
    return YGUnit(minWidthUnitsValue());
}

YGUnit LayoutSizingStyle::minHeightUnits()
{
    return YGUnit(minHeightUnitsValue());
}

void LayoutSizingStyle::applyBaseStyle(YGStyle& style,
                                       const LayoutSyncContext& context)
{
    style.display() = display();

    style.minDimensions()[YGDimensionWidth] =
        YGValue{minWidth(), minWidthUnits()};
    style.minDimensions()[YGDimensionHeight] =
        YGValue{minHeight(), minHeightUnits()};
    style.maxDimensions()[YGDimensionWidth] =
        YGValue{maxWidth(), maxWidthUnits()};
    style.maxDimensions()[YGDimensionHeight] =
        YGValue{maxHeight(), maxHeightUnits()};
}

// How this item sits in a grid or stack cell.
void LayoutSizingStyle::applyItemStyle(YGStyle& style,
                                       const LayoutSyncContext& context)
{
    if (context.parentIsStack)
    {
        GridTrack::syncStackItemCell(style);
    }
    GridTrack::syncItemJustifySelf(style,
                                   justifySelfValue(),
                                   context.parentIsStack,
                                   context.inlineHugs,
                                   context.containerJustifyItems);
    // Fill stretches the inline axis over the container's default justify.
    if (context.parentIsGrid &&
        layoutWidthScaleType() == (uint32_t)LayoutScaleType::fill)
    {
        style.setJustifySelf(YGJustifyStretch);
    }
}
#endif
