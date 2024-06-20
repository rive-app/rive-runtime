#ifndef _RIVE_LAYOUT_COMPONENT_STYLE_HPP_
#define _RIVE_LAYOUT_COMPONENT_STYLE_HPP_
#include "rive/generated/layout/layout_component_style_base.hpp"
#include "rive/math/bit_field_loc.hpp"
#ifdef WITH_RIVE_LAYOUT
#include "yoga/Yoga.h"
#endif
#include <stdio.h>
namespace rive
{
// ---- Flags 0
extern BitFieldLoc DisplayBits;
extern BitFieldLoc PositionTypeBits;
extern BitFieldLoc FlexDirectionBits;
extern BitFieldLoc DirectionBits;
extern BitFieldLoc AlignContentBits;
extern BitFieldLoc AlignItemsBits;
extern BitFieldLoc AlignSelfBits;
extern BitFieldLoc JustifyContentBits;
extern BitFieldLoc FlexWrapBits;
extern BitFieldLoc OverflowBits;
extern BitFieldLoc IntrinsicallySizedBits;
extern BitFieldLoc WidthUnitsBits;
extern BitFieldLoc HeightUnitsBits;

// ---- Flags 1
extern BitFieldLoc BorderLeftUnitsBits;
extern BitFieldLoc BorderRightUnitsBits;
extern BitFieldLoc BorderTopUnitsBits;
extern BitFieldLoc BorderBottomUnitsBits;
extern BitFieldLoc MarginLeftUnitsBits;
extern BitFieldLoc MarginRightUnitsBits;
extern BitFieldLoc MarginTopUnitsBits;
extern BitFieldLoc MarginBottomUnitsBits;
extern BitFieldLoc PaddingLeftUnitsBits;
extern BitFieldLoc PaddingRightUnitsBits;
extern BitFieldLoc PaddingTopUnitsBits;
extern BitFieldLoc PaddingBottomUnitsBits;
extern BitFieldLoc PositionLeftUnitsBits;
extern BitFieldLoc PositionRightUnitsBits;
extern BitFieldLoc PositionTopUnitsBits;
extern BitFieldLoc PositionBottomUnitsBits;

// ---- Flags 2
extern BitFieldLoc GapHorizontalUnitsBits;
extern BitFieldLoc GapVerticalUnitsBits;
extern BitFieldLoc MinWidthUnitsBits;
extern BitFieldLoc MinHeightUnitsBits;
extern BitFieldLoc MaxWidthUnitsBits;
extern BitFieldLoc MaxHeightUnitsBits;

class LayoutComponentStyle : public LayoutComponentStyleBase
{
public:
    LayoutComponentStyle() {}

#ifdef WITH_RIVE_LAYOUT
    YGDisplay display();
    YGPositionType positionType();

    YGFlexDirection flexDirection();
    YGDirection direction();
    YGWrap flexWrap();
    YGOverflow overflow();

    YGAlign alignItems();
    YGAlign alignSelf();
    YGAlign alignContent();
    YGJustify justifyContent();
    bool intrinsicallySized();
    YGUnit widthUnits();
    YGUnit heightUnits();

    YGUnit borderLeftUnits();
    YGUnit borderRightUnits();
    YGUnit borderTopUnits();
    YGUnit borderBottomUnits();
    YGUnit marginLeftUnits();
    YGUnit marginRightUnits();
    YGUnit marginTopUnits();
    YGUnit marginBottomUnits();
    YGUnit paddingLeftUnits();
    YGUnit paddingRightUnits();
    YGUnit paddingTopUnits();
    YGUnit paddingBottomUnits();
    YGUnit positionLeftUnits();
    YGUnit positionRightUnits();
    YGUnit positionTopUnits();
    YGUnit positionBottomUnits();

    YGUnit gapHorizontalUnits();
    YGUnit gapVerticalUnits();
    YGUnit maxWidthUnits();
    YGUnit maxHeightUnits();
    YGUnit minWidthUnits();
    YGUnit minHeightUnits();
#endif

    void markLayoutNodeDirty();

    void layoutFlags0Changed() override;
    void layoutFlags1Changed() override;
    void layoutFlags2Changed() override;
    void flexChanged() override;
    void flexGrowChanged() override;
    void flexShrinkChanged() override;
    void flexBasisChanged() override;
    void aspectRatioChanged() override;
    void gapHorizontalChanged() override;
    void gapVerticalChanged() override;
    void maxWidthChanged() override;
    void maxHeightChanged() override;
    void minWidthChanged() override;
    void minHeightChanged() override;
    void borderLeftChanged() override;
    void borderRightChanged() override;
    void borderTopChanged() override;
    void borderBottomChanged() override;
    void marginLeftChanged() override;
    void marginRightChanged() override;
    void marginTopChanged() override;
    void marginBottomChanged() override;
    void paddingLeftChanged() override;
    void paddingRightChanged() override;
    void paddingTopChanged() override;
    void paddingBottomChanged() override;
    void positionLeftChanged() override;
    void positionRightChanged() override;
    void positionTopChanged() override;
    void positionBottomChanged() override;
};
} // namespace rive

#endif