#include "rive/layout_component.hpp"
#include "rive/layout/layout_component_style.hpp"
#include <vector>

using namespace rive;

#ifdef WITH_RIVE_LAYOUT
YGDisplay LayoutComponentStyle::display() { return YGDisplay(DisplayBits.read(layoutFlags0())); }

YGPositionType LayoutComponentStyle::positionType()
{
    return YGPositionType(PositionTypeBits.read(layoutFlags0()));
}

YGFlexDirection LayoutComponentStyle::flexDirection()
{
    return YGFlexDirection(FlexDirectionBits.read(layoutFlags0()));
}

YGDirection LayoutComponentStyle::direction()
{
    return YGDirection(DirectionBits.read(layoutFlags0()));
}

YGWrap LayoutComponentStyle::flexWrap() { return YGWrap(FlexWrapBits.read(layoutFlags0())); }

YGAlign LayoutComponentStyle::alignItems() { return YGAlign(AlignItemsBits.read(layoutFlags0())); }

YGAlign LayoutComponentStyle::alignSelf() { return YGAlign(AlignSelfBits.read(layoutFlags0())); }

YGAlign LayoutComponentStyle::alignContent()
{
    return YGAlign(AlignContentBits.read(layoutFlags0()));
}

YGJustify LayoutComponentStyle::justifyContent()
{
    return YGJustify(JustifyContentBits.read(layoutFlags0()));
}

YGOverflow LayoutComponentStyle::overflow()
{
    return YGOverflow(OverflowBits.read(layoutFlags0()));
}

bool LayoutComponentStyle::intrinsicallySized()
{
    return IntrinsicallySizedBits.read(layoutFlags0()) == 1;
}

YGUnit LayoutComponentStyle::widthUnits() { return YGUnit(WidthUnitsBits.read(layoutFlags0())); }

YGUnit LayoutComponentStyle::heightUnits() { return YGUnit(HeightUnitsBits.read(layoutFlags0())); }

YGUnit LayoutComponentStyle::borderLeftUnits()
{
    return YGUnit(BorderLeftUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::borderRightUnits()
{
    return YGUnit(BorderRightUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::borderTopUnits()
{
    return YGUnit(BorderTopUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::borderBottomUnits()
{
    return YGUnit(BorderBottomUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::marginLeftUnits()
{
    return YGUnit(MarginLeftUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::marginRightUnits()
{
    return YGUnit(MarginRightUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::marginTopUnits()
{
    return YGUnit(MarginTopUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::marginBottomUnits()
{
    return YGUnit(MarginBottomUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::paddingLeftUnits()
{
    return YGUnit(PaddingLeftUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::paddingRightUnits()
{
    return YGUnit(PaddingRightUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::paddingTopUnits()
{
    return YGUnit(PaddingTopUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::paddingBottomUnits()
{
    return YGUnit(PaddingBottomUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::positionLeftUnits()
{
    return YGUnit(PositionLeftUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::positionRightUnits()
{
    return YGUnit(PositionRightUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::positionTopUnits()
{
    return YGUnit(PositionTopUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::positionBottomUnits()
{
    return YGUnit(PositionBottomUnitsBits.read(layoutFlags1()));
}

YGUnit LayoutComponentStyle::gapHorizontalUnits()
{
    return YGUnit(GapHorizontalUnitsBits.read(layoutFlags2()));
}

YGUnit LayoutComponentStyle::gapVerticalUnits()
{
    return YGUnit(GapVerticalUnitsBits.read(layoutFlags2()));
}

YGUnit LayoutComponentStyle::maxWidthUnits()
{
    return YGUnit(MaxWidthUnitsBits.read(layoutFlags2()));
}

YGUnit LayoutComponentStyle::maxHeightUnits()
{
    return YGUnit(MaxHeightUnitsBits.read(layoutFlags2()));
}

YGUnit LayoutComponentStyle::minWidthUnits()
{
    return YGUnit(MinWidthUnitsBits.read(layoutFlags2()));
}
YGUnit LayoutComponentStyle::minHeightUnits()
{
    return YGUnit(MinHeightUnitsBits.read(layoutFlags2()));
}
void LayoutComponentStyle::markLayoutNodeDirty()
{
    if (parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->markLayoutNodeDirty();
    }
}
#else
void LayoutComponentStyle::markLayoutNodeDirty() {}
#endif

void LayoutComponentStyle::layoutFlags0Changed() { markLayoutNodeDirty(); }
void LayoutComponentStyle::layoutFlags1Changed() { markLayoutNodeDirty(); }
void LayoutComponentStyle::layoutFlags2Changed() { markLayoutNodeDirty(); }
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