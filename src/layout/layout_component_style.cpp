#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/core_context.hpp"
#include "rive/layout_component.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout/layout_style_applier.hpp"
#include "rive/layout/grid_track.hpp"
#ifdef WITH_RIVE_LAYOUT
// YGStyle is only forward declared in the headers; the setters below need it.
#include "rive/layout/layout_data.hpp"
#endif
#include <vector>

using namespace rive;

#ifdef WITH_RIVE_LAYOUT
// Stack alignment: the 9-way selector sets the default cell placement via grid
// justifyItems (horizontal) + alignItems (vertical). Per-child
// justifySelf/alignSelf still overrides. space-between collapses to start.
static void applyStackAlignment(YGStyle& ygStyle, LayoutAlignmentType t)
{
    switch (t)
    {
        case LayoutAlignmentType::topLeft:
        case LayoutAlignmentType::centerLeft:
        case LayoutAlignmentType::bottomLeft:
        case LayoutAlignmentType::spaceBetweenStart:
            ygStyle.setJustifyItems(YGJustifyStart);
            break;
        case LayoutAlignmentType::topCenter:
        case LayoutAlignmentType::center:
        case LayoutAlignmentType::bottomCenter:
        case LayoutAlignmentType::spaceBetweenCenter:
            ygStyle.setJustifyItems(YGJustifyCenter);
            break;
        case LayoutAlignmentType::topRight:
        case LayoutAlignmentType::centerRight:
        case LayoutAlignmentType::bottomRight:
        case LayoutAlignmentType::spaceBetweenEnd:
            ygStyle.setJustifyItems(YGJustifyEnd);
            break;
    }
    switch (t)
    {
        case LayoutAlignmentType::topLeft:
        case LayoutAlignmentType::topCenter:
        case LayoutAlignmentType::topRight:
            ygStyle.alignItems() = YGAlignStart;
            break;
        case LayoutAlignmentType::centerLeft:
        case LayoutAlignmentType::center:
        case LayoutAlignmentType::centerRight:
            ygStyle.alignItems() = YGAlignCenter;
            break;
        case LayoutAlignmentType::bottomLeft:
        case LayoutAlignmentType::bottomCenter:
        case LayoutAlignmentType::bottomRight:
            ygStyle.alignItems() = YGAlignEnd;
            break;
        case LayoutAlignmentType::spaceBetweenStart:
        case LayoutAlignmentType::spaceBetweenCenter:
        case LayoutAlignmentType::spaceBetweenEnd:
            ygStyle.alignItems() = YGAlignStart;
            break;
    }
}
#endif

#ifdef WITH_RIVE_LAYOUT
// How this layout sits inside its own parent.
void LayoutComponentStyle::applyItemStyle(YGStyle& style,
                                          const LayoutSyncContext& context)
{
    LayoutSizingStyle::applyItemStyle(style, context);

    style.positionType() = positionType();
    style.aspectRatio() =
        YGFloatOptional(aspectRatio() > 0 ? aspectRatio() : NAN);

    const auto startEdge = context.isLTR ? YGEdgeLeft : YGEdgeRight;
    const auto endEdge = context.isLTR ? YGEdgeRight : YGEdgeLeft;
    // No parent to take a percentage of, so margins resolve as points.
    const auto marginUnits = [&](YGUnit units) {
        return context.hasLayoutParent ? units : YGUnitPoint;
    };

    style.margin()[startEdge] =
        YGValue{marginLeft(), marginUnits(marginLeftUnits())};
    style.margin()[endEdge] =
        YGValue{marginRight(), marginUnits(marginRightUnits())};
    style.margin()[YGEdgeTop] =
        YGValue{marginTop(), marginUnits(marginTopUnits())};
    style.margin()[YGEdgeBottom] =
        YGValue{marginBottom(), marginUnits(marginBottomUnits())};

    style.position()[startEdge] = YGValue{positionLeft(), positionLeftUnits()};
    style.position()[endEdge] = YGValue{positionRight(), positionRightUnits()};
    style.position()[YGEdgeTop] = YGValue{positionTop(), positionTopUnits()};
    style.position()[YGEdgeBottom] =
        YGValue{positionBottom(), positionBottomUnits()};
}
#endif

#ifdef WITH_RIVE_LAYOUT
// What this layout imposes on its children: their box (padding/border/gap) and
// the 9-way alignment selector. The two switches below split horizontal from
// vertical intent, since the axis each lands on follows the main axis.
void LayoutComponentStyle::applyContainerStyle(YGStyle& style,
                                               const LayoutSyncContext& context)
{
    const auto startEdge = context.isLTR ? YGEdgeLeft : YGEdgeRight;
    const auto endEdge = context.isLTR ? YGEdgeRight : YGEdgeLeft;

    style.flexDirection() = flexDirection();
    style.flexWrap() = flexWrap();
    style.direction() = direction();

    style.gap()[YGGutterColumn] =
        YGValue{gapHorizontal(), gapHorizontalUnits()};
    style.gap()[YGGutterRow] = YGValue{gapVertical(), gapVerticalUnits()};

    style.border()[startEdge] = YGValue{borderLeft(), borderLeftUnits()};
    style.border()[endEdge] = YGValue{borderRight(), borderRightUnits()};
    style.border()[YGEdgeTop] = YGValue{borderTop(), borderTopUnits()};
    style.border()[YGEdgeBottom] = YGValue{borderBottom(), borderBottomUnits()};

    style.padding()[startEdge] = YGValue{paddingLeft(), paddingLeftUnits()};
    style.padding()[endEdge] = YGValue{paddingRight(), paddingRightUnits()};
    style.padding()[YGEdgeTop] = YGValue{paddingTop(), paddingTopUnits()};
    style.padding()[YGEdgeBottom] =
        YGValue{paddingBottom(), paddingBottomUnits()};

    // A stack is one implicit cell, aligned through justifyItems/alignItems
    // rather than the flex selector below.
    if (isStack())
    {
        GridTrack::syncStackContainerStyle(style, justifyItemsValue());
        applyStackAlignment(style, alignmentType());
        return;
    }
    // Our own main axis, not the parent's: this aligns the children we lay out.
    const bool isRowForAlignment = flexDirection() == YGFlexDirectionRow ||
                                   flexDirection() == YGFlexDirectionRowReverse;
    switch (alignmentType())
    {
        case LayoutAlignmentType::topLeft:
        case LayoutAlignmentType::topCenter:
        case LayoutAlignmentType::topRight:
            if (isRowForAlignment)
            {
                style.alignItems() = YGAlignFlexStart;
                style.alignContent() = YGAlignFlexStart;
            }
            else
            {
                style.justifyContent() = YGJustifyFlexStart;
            }
            break;
        case LayoutAlignmentType::centerLeft:
        case LayoutAlignmentType::center:
        case LayoutAlignmentType::centerRight:
            if (isRowForAlignment)
            {
                style.alignItems() = YGAlignCenter;
                style.alignContent() = YGAlignCenter;
            }
            else
            {
                style.justifyContent() = YGJustifyCenter;
            }
            break;
        case LayoutAlignmentType::bottomLeft:
        case LayoutAlignmentType::bottomCenter:
        case LayoutAlignmentType::bottomRight:
            if (isRowForAlignment)
            {
                style.alignItems() = YGAlignFlexEnd;
                style.alignContent() = YGAlignFlexEnd;
            }
            else
            {
                style.justifyContent() = YGJustifyFlexEnd;
            }
            break;
        default:
            break;
    }
    switch (alignmentType())
    {
        case LayoutAlignmentType::topLeft:
        case LayoutAlignmentType::centerLeft:
        case LayoutAlignmentType::bottomLeft:
            if (isRowForAlignment)
            {
                style.justifyContent() = YGJustifyFlexStart;
            }
            else
            {
                style.alignItems() = YGAlignFlexStart;
                style.alignContent() = YGAlignFlexStart;
            }
            break;
        case LayoutAlignmentType::topCenter:
        case LayoutAlignmentType::center:
        case LayoutAlignmentType::bottomCenter:
            if (isRowForAlignment)
            {
                style.justifyContent() = YGJustifyCenter;
            }
            else
            {
                style.alignItems() = YGAlignCenter;
                style.alignContent() = YGAlignCenter;
            }
            break;
        case LayoutAlignmentType::topRight:
        case LayoutAlignmentType::centerRight:
        case LayoutAlignmentType::bottomRight:
            if (isRowForAlignment)
            {
                style.justifyContent() = YGJustifyFlexEnd;
            }
            else
            {
                style.alignItems() = YGAlignFlexEnd;
                style.alignContent() = YGAlignFlexEnd;
            }
            break;
        case LayoutAlignmentType::spaceBetweenStart:
            style.alignItems() = YGAlignFlexStart;
            style.alignContent() = YGAlignFlexStart;
            style.justifyContent() = YGJustifySpaceBetween;
            break;
        case LayoutAlignmentType::spaceBetweenCenter:
            style.alignItems() = YGAlignCenter;
            style.alignContent() = YGAlignCenter;
            style.justifyContent() = YGJustifySpaceBetween;
            break;
        case LayoutAlignmentType::spaceBetweenEnd:
            style.alignItems() = YGAlignFlexEnd;
            style.alignContent() = YGAlignFlexEnd;
            style.justifyContent() = YGJustifySpaceBetween;
            break;
    }
}
#endif

#ifdef WITH_RIVE_LAYOUT

KeyFrameInterpolator* LayoutComponentStyle::interpolator()
{
    return m_interpolator;
}

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

YGDisplay LayoutComponentStyle::display()
{
    // displayValue is visibility only (flex/none); layoutTypeValue selects the
    // algorithm. Hidden wins, else flex or grid (stack also drives the grid).
    if (YGDisplay(displayValue()) == YGDisplayNone)
    {
        return YGDisplayNone;
    }
    return layoutTypeValue() == 0 ? YGDisplayFlex : YGDisplayGrid;
}

YGPositionType LayoutComponentStyle::positionType()
{
    return YGPositionType(positionTypeValue());
}

YGFlexDirection LayoutComponentStyle::flexDirection()
{
    return YGFlexDirection(flexDirectionValue());
}

YGDirection LayoutComponentStyle::direction()
{
    return YGDirection(directionValue());
}

YGWrap LayoutComponentStyle::flexWrap() { return YGWrap(flexWrapValue()); }

YGOverflow LayoutComponentStyle::overflow()
{
    return YGOverflow(overflowValue());
}

bool LayoutComponentStyle::intrinsicallySized()
{
    return intrinsicallySizedValue() == 1;
}

YGUnit LayoutComponentStyle::widthUnits() { return YGUnit(widthUnitsValue()); }

YGUnit LayoutComponentStyle::heightUnits()
{
    return YGUnit(heightUnitsValue());
}

YGUnit LayoutComponentStyle::borderLeftUnits()
{
    return YGUnit(borderLeftUnitsValue());
}

YGUnit LayoutComponentStyle::borderRightUnits()
{
    return YGUnit(borderRightUnitsValue());
}

YGUnit LayoutComponentStyle::borderTopUnits()
{
    return YGUnit(borderTopUnitsValue());
}

YGUnit LayoutComponentStyle::borderBottomUnits()
{
    return YGUnit(borderBottomUnitsValue());
}

YGUnit LayoutComponentStyle::marginLeftUnits()
{
    return YGUnit(marginLeftUnitsValue());
}

YGUnit LayoutComponentStyle::marginRightUnits()
{
    return YGUnit(marginRightUnitsValue());
}

YGUnit LayoutComponentStyle::marginTopUnits()
{
    return YGUnit(marginTopUnitsValue());
}

YGUnit LayoutComponentStyle::marginBottomUnits()
{
    return YGUnit(marginBottomUnitsValue());
}

YGUnit LayoutComponentStyle::paddingLeftUnits()
{
    return YGUnit(paddingLeftUnitsValue());
}

YGUnit LayoutComponentStyle::paddingRightUnits()
{
    return YGUnit(paddingRightUnitsValue());
}

YGUnit LayoutComponentStyle::paddingTopUnits()
{
    return YGUnit(paddingTopUnitsValue());
}

YGUnit LayoutComponentStyle::paddingBottomUnits()
{
    return YGUnit(paddingBottomUnitsValue());
}

YGUnit LayoutComponentStyle::positionLeftUnits()
{
    return YGUnit(positionLeftUnitsValue());
}

YGUnit LayoutComponentStyle::positionRightUnits()
{
    return YGUnit(positionRightUnitsValue());
}

YGUnit LayoutComponentStyle::positionTopUnits()
{
    return YGUnit(positionTopUnitsValue());
}

YGUnit LayoutComponentStyle::positionBottomUnits()
{
    return YGUnit(positionBottomUnitsValue());
}

YGUnit LayoutComponentStyle::gapHorizontalUnits()
{
    return YGUnit(gapHorizontalUnitsValue());
}

YGUnit LayoutComponentStyle::gapVerticalUnits()
{
    return YGUnit(gapVerticalUnitsValue());
}

YGUnit LayoutComponentStyle::flexBasisUnits()
{
    return YGUnit(flexBasisUnitsValue());
}

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

void LayoutComponentStyle::scaleTypeChanged()
{
    if (parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->scaleTypeChanged();
    }
}

void LayoutComponentStyle::displayChanged()
{
    if (parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->displayChanged();
    }
}

void LayoutComponentStyle::positionTypeValueChanged()
{
    if (parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->positionTypeChanged();
    }
}

void LayoutComponentStyle::flexDirectionValueChanged()
{
    if (parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->flexDirectionChanged();
    }
}

void LayoutComponentStyle::directionValueChanged()
{
    if (parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->directionChanged();
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
void LayoutComponentStyle::scaleTypeChanged() {}
void LayoutComponentStyle::displayChanged() {}
void LayoutComponentStyle::positionTypeValueChanged() {}
void LayoutComponentStyle::flexDirectionValueChanged() {}
void LayoutComponentStyle::directionValueChanged() {}
#endif

void LayoutComponentStyle::interpolationTimeChanged()
{
    markLayoutStyleDirty();
}
void LayoutComponentStyle::layoutAlignmentTypeChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::layoutWidthScaleTypeChanged() { scaleTypeChanged(); }
void LayoutComponentStyle::layoutHeightScaleTypeChanged()
{
    scaleTypeChanged();
}
void LayoutComponentStyle::displayValueChanged() { displayChanged(); }
void LayoutComponentStyle::layoutTypeValueChanged()
{
#ifdef WITH_RIVE_LAYOUT
    if (parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->layoutTypeChanged();
    }
#endif
}
void LayoutComponentStyle::justifyItemsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::justifySelfValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::overflowValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::intrinsicallySizedValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::flexWrapValueChanged() { markLayoutNodeDirty(); }

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
void LayoutComponentStyle::positionLeftChanged()
{
    if (parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->markPositionLeftChanged();
    }
    markLayoutNodeDirty();
}
void LayoutComponentStyle::positionRightChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::positionTopChanged()
{
    if (parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->markPositionTopChanged();
    }
    markLayoutNodeDirty();
}
void LayoutComponentStyle::positionBottomChanged() { markLayoutNodeDirty(); }

void LayoutComponentStyle::widthUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::heightUnitsValueChanged() { markLayoutNodeDirty(); }
void LayoutComponentStyle::gapHorizontalUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::gapVerticalUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::maxWidthUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::maxHeightUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::minWidthUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::minHeightUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::borderLeftUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::borderRightUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::borderTopUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::borderBottomUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::marginLeftUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::marginRightUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::marginTopUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::marginBottomUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::paddingLeftUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::paddingRightUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::paddingTopUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::paddingBottomUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::positionLeftUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::positionRightUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::positionTopUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::positionBottomUnitsValueChanged()
{
    markLayoutNodeDirty();
}
void LayoutComponentStyle::cornerRadiusTLChanged() { markLayoutStyleDirty(); }
void LayoutComponentStyle::cornerRadiusTRChanged() { markLayoutStyleDirty(); }
void LayoutComponentStyle::cornerRadiusBLChanged() { markLayoutStyleDirty(); }
void LayoutComponentStyle::cornerRadiusBRChanged() { markLayoutStyleDirty(); }