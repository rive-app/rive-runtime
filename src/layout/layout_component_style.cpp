#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/core_context.hpp"
#include "rive/layout_component.hpp"
#include "rive/layout/layout_component_style.hpp"
#include <vector>

using namespace rive;

#ifdef WITH_RIVE_LAYOUT

KeyFrameInterpolator* LayoutComponentStyle::interpolator() { return m_interpolator; }

LayoutStyleInterpolation LayoutComponentStyle::interpolation()
{
    return LayoutStyleInterpolation(interpolationType());
}

LayoutAnimationStyle LayoutComponentStyle::animationStyle()
{
    return LayoutAnimationStyle(animationStyleType());
}

LayoutAlignmentType LayoutComponentStyle::alignmentType()
{
    return LayoutAlignmentType(layoutAlignmentType());
}

LayoutScaleType LayoutComponentStyle::widthScaleType()
{
    return LayoutScaleType(layoutWidthScaleType());
}

LayoutScaleType LayoutComponentStyle::heightScaleType()
{
    return LayoutScaleType(layoutHeightScaleType());
}

YGDisplay LayoutComponentStyle::display() { return YGDisplay(displayValue()); }

YGPositionType LayoutComponentStyle::positionType() { return YGPositionType(positionTypeValue()); }

YGFlexDirection LayoutComponentStyle::flexDirection()
{
    return YGFlexDirection(flexDirectionValue());
}

YGDirection LayoutComponentStyle::direction() { return YGDirection(directionValue()); }

YGWrap LayoutComponentStyle::flexWrap() { return YGWrap(flexWrapValue()); }

YGAlign LayoutComponentStyle::alignItems() { return YGAlign(alignItemsValue()); }

YGAlign LayoutComponentStyle::alignSelf() { return YGAlign(alignSelfValue()); }

YGAlign LayoutComponentStyle::alignContent() { return YGAlign(alignContentValue()); }

YGJustify LayoutComponentStyle::justifyContent() { return YGJustify(justifyContentValue()); }

YGOverflow LayoutComponentStyle::overflow() { return YGOverflow(overflowValue()); }

bool LayoutComponentStyle::intrinsicallySized() { return intrinsicallySizedValue() == 1; }

YGUnit LayoutComponentStyle::widthUnits() { return YGUnit(widthUnitsValue()); }

YGUnit LayoutComponentStyle::heightUnits() { return YGUnit(heightUnitsValue()); }

YGUnit LayoutComponentStyle::borderLeftUnits() { return YGUnit(borderLeftUnitsValue()); }

YGUnit LayoutComponentStyle::borderRightUnits() { return YGUnit(borderRightUnitsValue()); }

YGUnit LayoutComponentStyle::borderTopUnits() { return YGUnit(borderTopUnitsValue()); }

YGUnit LayoutComponentStyle::borderBottomUnits() { return YGUnit(borderBottomUnitsValue()); }

YGUnit LayoutComponentStyle::marginLeftUnits() { return YGUnit(marginLeftUnitsValue()); }

YGUnit LayoutComponentStyle::marginRightUnits() { return YGUnit(marginRightUnitsValue()); }

YGUnit LayoutComponentStyle::marginTopUnits() { return YGUnit(marginTopUnitsValue()); }

YGUnit LayoutComponentStyle::marginBottomUnits() { return YGUnit(marginBottomUnitsValue()); }

YGUnit LayoutComponentStyle::paddingLeftUnits() { return YGUnit(paddingLeftUnitsValue()); }

YGUnit LayoutComponentStyle::paddingRightUnits() { return YGUnit(paddingRightUnitsValue()); }

YGUnit LayoutComponentStyle::paddingTopUnits() { return YGUnit(paddingTopUnitsValue()); }

YGUnit LayoutComponentStyle::paddingBottomUnits() { return YGUnit(paddingBottomUnitsValue()); }

YGUnit LayoutComponentStyle::positionLeftUnits() { return YGUnit(positionLeftUnitsValue()); }

YGUnit LayoutComponentStyle::positionRightUnits() { return YGUnit(positionRightUnitsValue()); }

YGUnit LayoutComponentStyle::positionTopUnits() { return YGUnit(positionTopUnitsValue()); }

YGUnit LayoutComponentStyle::positionBottomUnits() { return YGUnit(positionBottomUnitsValue()); }

YGUnit LayoutComponentStyle::gapHorizontalUnits() { return YGUnit(gapHorizontalUnitsValue()); }

YGUnit LayoutComponentStyle::gapVerticalUnits() { return YGUnit(gapVerticalUnitsValue()); }

YGUnit LayoutComponentStyle::maxWidthUnits() { return YGUnit(maxWidthUnitsValue()); }

YGUnit LayoutComponentStyle::maxHeightUnits() { return YGUnit(maxHeightUnitsValue()); }

YGUnit LayoutComponentStyle::minWidthUnits() { return YGUnit(minWidthUnitsValue()); }
YGUnit LayoutComponentStyle::minHeightUnits() { return YGUnit(minHeightUnitsValue()); }
void LayoutComponentStyle::markLayoutNodeDirty()
{
    if (parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->markLayoutNodeDirty();
    }
}

void LayoutComponentStyle::markLayoutStyleDirty()
{
    if (parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->markLayoutStyleDirty();
    }
}

StatusCode LayoutComponentStyle::onAddedDirty(CoreContext* context)
{
    auto code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    auto coreObject = context->resolve(interpolatorId());
    if (coreObject != nullptr && coreObject->is<KeyFrameInterpolator>())
    {
        m_interpolator = static_cast<KeyFrameInterpolator*>(coreObject);
    }
    return StatusCode::Ok;
}
#else
void LayoutComponentStyle::markLayoutNodeDirty() {}
void LayoutComponentStyle::markLayoutStyleDirty() {}
#endif

void LayoutComponentStyle::layoutAlignmentTypeChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::layoutWidthScaleTypeChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::layoutHeightScaleTypeChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::displayValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::positionTypeValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::overflowValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::intrinsicallySizedValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::flexDirectionValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::directionValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::alignContentValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::alignItemsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::alignSelfValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::justifyContentValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::flexWrapValueChanged() { markLayoutNodeDirty(); }

void LayoutComponentStyle::flexChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::flexGrowChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::flexShrinkChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::flexBasisChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::aspectRatioChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::gapHorizontalChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::gapVerticalChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::maxWidthChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::maxHeightChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::minWidthChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::minHeightChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::borderLeftChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::borderRightChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::borderTopChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::borderBottomChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::marginLeftChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::marginRightChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::marginTopChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::marginBottomChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::paddingLeftChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::paddingRightChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::paddingTopChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::paddingBottomChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::positionLeftChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::positionRightChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::positionTopChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::positionBottomChanged() { markLayoutNodeDirty(); }

void LayoutComponentStyle::widthUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::heightUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::gapHorizontalUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::gapVerticalUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::maxWidthUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::maxHeightUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::minWidthUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::minHeightUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::borderLeftUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::borderRightUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::borderTopUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::borderBottomUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::marginLeftUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::marginRightUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::marginTopUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::marginBottomUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::paddingLeftUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::paddingRightUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::paddingTopUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::paddingBottomUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::positionLeftUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::positionRightUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::positionTopUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::positionBottomUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::cornerRadiusTLChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::cornerRadiusTRChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::cornerRadiusBLChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::cornerRadiusBRChanged() { markLayoutNodeDirty(); }