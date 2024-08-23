#ifndef _RIVE_LAYOUT_COMPONENT_STYLE_HPP_
#define _RIVE_LAYOUT_COMPONENT_STYLE_HPP_
#include "rive/generated/layout/layout_component_style_base.hpp"
#ifdef WITH_RIVE_LAYOUT
#include "yoga/Yoga.h"
#endif
#include <stdio.h>
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

enum class LayoutScaleType : uint8_t
{
    fixed,
    fill,
    hug
};

class KeyFrameInterpolator;
class LayoutComponentStyle : public LayoutComponentStyleBase
{
private:
#ifdef WITH_RIVE_LAYOUT
    KeyFrameInterpolator* m_interpolator;
#endif

public:
    LayoutComponentStyle() {}

#ifdef WITH_RIVE_LAYOUT
    StatusCode onAddedDirty(CoreContext* context) override;
    KeyFrameInterpolator* interpolator();
    LayoutStyleInterpolation interpolation();
    LayoutAnimationStyle animationStyle();
    YGDisplay display();
    YGPositionType positionType();
    LayoutAlignmentType alignmentType();
    LayoutScaleType widthScaleType();
    LayoutScaleType heightScaleType();

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
    void markLayoutStyleDirty();

    void layoutAlignmentTypeChanged() override;
    void layoutWidthScaleTypeChanged() override;
    void layoutHeightScaleTypeChanged() override;
    void displayValueChanged() override;
    void positionTypeValueChanged() override;
    void overflowValueChanged() override;
    void intrinsicallySizedValueChanged() override;
    void flexDirectionValueChanged() override;
    void directionValueChanged() override;
    void alignContentValueChanged() override;
    void alignItemsValueChanged() override;
    void alignSelfValueChanged() override;
    void justifyContentValueChanged() override;
    void flexWrapValueChanged() override;
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

    void widthUnitsValueChanged() override;
    void heightUnitsValueChanged() override;
    void gapHorizontalUnitsValueChanged() override;
    void gapVerticalUnitsValueChanged() override;
    void maxWidthUnitsValueChanged() override;
    void maxHeightUnitsValueChanged() override;
    void minWidthUnitsValueChanged() override;
    void minHeightUnitsValueChanged() override;
    void borderLeftUnitsValueChanged() override;
    void borderRightUnitsValueChanged() override;
    void borderTopUnitsValueChanged() override;
    void borderBottomUnitsValueChanged() override;
    void marginLeftUnitsValueChanged() override;
    void marginRightUnitsValueChanged() override;
    void marginTopUnitsValueChanged() override;
    void marginBottomUnitsValueChanged() override;
    void paddingLeftUnitsValueChanged() override;
    void paddingRightUnitsValueChanged() override;
    void paddingTopUnitsValueChanged() override;
    void paddingBottomUnitsValueChanged() override;
    void positionLeftUnitsValueChanged() override;
    void positionRightUnitsValueChanged() override;
    void positionTopUnitsValueChanged() override;
    void positionBottomUnitsValueChanged() override;

    void cornerRadiusTLChanged() override;
    void cornerRadiusTRChanged() override;
    void cornerRadiusBLChanged() override;
    void cornerRadiusBRChanged() override;
};
} // namespace rive

#endif