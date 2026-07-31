#ifndef _RIVE_LAYOUT_COMPONENT_STYLE_HPP_
#define _RIVE_LAYOUT_COMPONENT_STYLE_HPP_
#include "rive/generated/layout/layout_component_style_base.hpp"
#include "rive/layout/layout_style_applier.hpp"
#include "rive/layout/layout_enums.hpp"
#ifndef TESTING
#include "rive/internal/assert_internal_only.hpp"
#endif
#ifdef WITH_RIVE_LAYOUT
#include "yoga/Yoga.h"
#endif
#include <stdio.h>
namespace rive
{

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
    YGDisplay display() override;
    // Stack is realized as a 1x1 grid with all children in cell 1,1.
    bool isStack() const { return layoutTypeValue() == 2; }
    // Grid-like: children lay out on a grid (grid or stack).
    bool isGrid() const { return layoutTypeValue() != 0; }
    YGPositionType positionType();
    LayoutAlignmentType alignmentType();
    LayoutScaleType widthScaleType();
    LayoutScaleType heightScaleType();

    YGFlexDirection flexDirection();
    YGDirection direction();
    YGWrap flexWrap();
    YGOverflow overflow();

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
    YGUnit flexBasisUnits();
#endif

#ifdef WITH_RIVE_LAYOUT
    void applyContainerStyle(YGStyle& style,
                             const LayoutSyncContext& context) override;
    void applyItemStyle(YGStyle& style,
                        const LayoutSyncContext& context) override;
#endif
    void markLayoutNodeDirty();
    void markLayoutStyleDirty();
    void scaleTypeChanged();
    void displayChanged();

    void interpolationTimeChanged() override;
    void layoutAlignmentTypeChanged() override;
    void layoutWidthScaleTypeChanged() override;
    void layoutHeightScaleTypeChanged() override;
    void displayValueChanged() override;
    void layoutTypeValueChanged() override;
    void justifyItemsValueChanged() override;
    void justifySelfValueChanged() override;
    void positionTypeValueChanged() override;
    void overflowValueChanged() override;
    void intrinsicallySizedValueChanged() override;
    void flexDirectionValueChanged() override;
    void directionValueChanged() override;
    void flexWrapValueChanged() override;
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