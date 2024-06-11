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
static BitFieldLoc DisplayBits = BitFieldLoc(0, 0);
static BitFieldLoc PositionTypeBits = BitFieldLoc(1, 2);
static BitFieldLoc FlexDirectionBits = BitFieldLoc(3, 4);
static BitFieldLoc DirectionBits = BitFieldLoc(5, 6);
static BitFieldLoc AlignContentBits = BitFieldLoc(7, 9);
static BitFieldLoc AlignItemsBits = BitFieldLoc(10, 12);
static BitFieldLoc AlignSelfBits = BitFieldLoc(13, 15);
static BitFieldLoc JustifyContentBits = BitFieldLoc(16, 18);
static BitFieldLoc FlexWrapBits = BitFieldLoc(19, 20);
static BitFieldLoc OverflowBits = BitFieldLoc(21, 22);
static BitFieldLoc IntrinsicallySizedBits = BitFieldLoc(23, 23);
static BitFieldLoc WidthUnitsBits = BitFieldLoc(24, 25);
static BitFieldLoc HeightUnitsBits = BitFieldLoc(26, 27);

// ---- Flags 1
static BitFieldLoc BorderLeftUnitsBits = BitFieldLoc(0, 1);
static BitFieldLoc BorderRightUnitsBits = BitFieldLoc(2, 3);
static BitFieldLoc BorderTopUnitsBits = BitFieldLoc(4, 5);
static BitFieldLoc BorderBottomUnitsBits = BitFieldLoc(6, 7);
static BitFieldLoc MarginLeftUnitsBits = BitFieldLoc(8, 9);
static BitFieldLoc MarginRightUnitsBits = BitFieldLoc(10, 11);
static BitFieldLoc MarginTopUnitsBits = BitFieldLoc(12, 13);
static BitFieldLoc MarginBottomUnitsBits = BitFieldLoc(14, 15);
static BitFieldLoc PaddingLeftUnitsBits = BitFieldLoc(16, 17);
static BitFieldLoc PaddingRightUnitsBits = BitFieldLoc(18, 19);
static BitFieldLoc PaddingTopUnitsBits = BitFieldLoc(20, 21);
static BitFieldLoc PaddingBottomUnitsBits = BitFieldLoc(22, 23);
static BitFieldLoc PositionLeftUnitsBits = BitFieldLoc(24, 25);
static BitFieldLoc PositionRightUnitsBits = BitFieldLoc(26, 27);
static BitFieldLoc PositionTopUnitsBits = BitFieldLoc(28, 29);
static BitFieldLoc PositionBottomUnitsBits = BitFieldLoc(30, 31);

// ---- Flags 2
static BitFieldLoc GapHorizontalUnitsBits = BitFieldLoc(0, 1);
static BitFieldLoc GapVerticalUnitsBits = BitFieldLoc(2, 3);
static BitFieldLoc MinWidthUnitsBits = BitFieldLoc(4, 5);
static BitFieldLoc MinHeightUnitsBits = BitFieldLoc(6, 7);
static BitFieldLoc MaxWidthUnitsBits = BitFieldLoc(8, 9);
static BitFieldLoc MaxHeightUnitsBits = BitFieldLoc(10, 11);

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