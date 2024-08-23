#ifndef _RIVE_LAYOUT_COMPONENT_STYLE_BASE_HPP_
#define _RIVE_LAYOUT_COMPONENT_STYLE_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class LayoutComponentStyleBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 420;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case LayoutComponentStyleBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t gapHorizontalPropertyKey = 498;
    static const uint16_t gapVerticalPropertyKey = 499;
    static const uint16_t maxWidthPropertyKey = 500;
    static const uint16_t maxHeightPropertyKey = 501;
    static const uint16_t minWidthPropertyKey = 502;
    static const uint16_t minHeightPropertyKey = 503;
    static const uint16_t borderLeftPropertyKey = 504;
    static const uint16_t borderRightPropertyKey = 505;
    static const uint16_t borderTopPropertyKey = 506;
    static const uint16_t borderBottomPropertyKey = 507;
    static const uint16_t marginLeftPropertyKey = 508;
    static const uint16_t marginRightPropertyKey = 509;
    static const uint16_t marginTopPropertyKey = 510;
    static const uint16_t marginBottomPropertyKey = 511;
    static const uint16_t paddingLeftPropertyKey = 512;
    static const uint16_t paddingRightPropertyKey = 513;
    static const uint16_t paddingTopPropertyKey = 514;
    static const uint16_t paddingBottomPropertyKey = 515;
    static const uint16_t positionLeftPropertyKey = 516;
    static const uint16_t positionRightPropertyKey = 517;
    static const uint16_t positionTopPropertyKey = 518;
    static const uint16_t positionBottomPropertyKey = 519;
    static const uint16_t flexPropertyKey = 520;
    static const uint16_t flexGrowPropertyKey = 521;
    static const uint16_t flexShrinkPropertyKey = 522;
    static const uint16_t flexBasisPropertyKey = 523;
    static const uint16_t aspectRatioPropertyKey = 524;
    static const uint16_t layoutWidthScaleTypePropertyKey = 655;
    static const uint16_t layoutHeightScaleTypePropertyKey = 656;
    static const uint16_t layoutAlignmentTypePropertyKey = 632;
    static const uint16_t animationStyleTypePropertyKey = 589;
    static const uint16_t interpolationTypePropertyKey = 590;
    static const uint16_t interpolatorIdPropertyKey = 591;
    static const uint16_t interpolationTimePropertyKey = 592;
    static const uint16_t displayValuePropertyKey = 596;
    static const uint16_t positionTypeValuePropertyKey = 597;
    static const uint16_t flexDirectionValuePropertyKey = 598;
    static const uint16_t directionValuePropertyKey = 599;
    static const uint16_t alignContentValuePropertyKey = 600;
    static const uint16_t alignItemsValuePropertyKey = 601;
    static const uint16_t alignSelfValuePropertyKey = 602;
    static const uint16_t justifyContentValuePropertyKey = 603;
    static const uint16_t flexWrapValuePropertyKey = 604;
    static const uint16_t overflowValuePropertyKey = 605;
    static const uint16_t intrinsicallySizedValuePropertyKey = 606;
    static const uint16_t widthUnitsValuePropertyKey = 607;
    static const uint16_t heightUnitsValuePropertyKey = 608;
    static const uint16_t borderLeftUnitsValuePropertyKey = 609;
    static const uint16_t borderRightUnitsValuePropertyKey = 610;
    static const uint16_t borderTopUnitsValuePropertyKey = 611;
    static const uint16_t borderBottomUnitsValuePropertyKey = 612;
    static const uint16_t marginLeftUnitsValuePropertyKey = 613;
    static const uint16_t marginRightUnitsValuePropertyKey = 614;
    static const uint16_t marginTopUnitsValuePropertyKey = 615;
    static const uint16_t marginBottomUnitsValuePropertyKey = 616;
    static const uint16_t paddingLeftUnitsValuePropertyKey = 617;
    static const uint16_t paddingRightUnitsValuePropertyKey = 618;
    static const uint16_t paddingTopUnitsValuePropertyKey = 619;
    static const uint16_t paddingBottomUnitsValuePropertyKey = 620;
    static const uint16_t positionLeftUnitsValuePropertyKey = 621;
    static const uint16_t positionRightUnitsValuePropertyKey = 622;
    static const uint16_t positionTopUnitsValuePropertyKey = 623;
    static const uint16_t positionBottomUnitsValuePropertyKey = 624;
    static const uint16_t gapHorizontalUnitsValuePropertyKey = 625;
    static const uint16_t gapVerticalUnitsValuePropertyKey = 626;
    static const uint16_t minWidthUnitsValuePropertyKey = 627;
    static const uint16_t minHeightUnitsValuePropertyKey = 628;
    static const uint16_t maxWidthUnitsValuePropertyKey = 629;
    static const uint16_t maxHeightUnitsValuePropertyKey = 630;
    static const uint16_t linkCornerRadiusPropertyKey = 639;
    static const uint16_t cornerRadiusTLPropertyKey = 640;
    static const uint16_t cornerRadiusTRPropertyKey = 641;
    static const uint16_t cornerRadiusBLPropertyKey = 642;
    static const uint16_t cornerRadiusBRPropertyKey = 643;

private:
    float m_GapHorizontal = 0.0f;
    float m_GapVertical = 0.0f;
    float m_MaxWidth = 0.0f;
    float m_MaxHeight = 0.0f;
    float m_MinWidth = 0.0f;
    float m_MinHeight = 0.0f;
    float m_BorderLeft = 0.0f;
    float m_BorderRight = 0.0f;
    float m_BorderTop = 0.0f;
    float m_BorderBottom = 0.0f;
    float m_MarginLeft = 0.0f;
    float m_MarginRight = 0.0f;
    float m_MarginTop = 0.0f;
    float m_MarginBottom = 0.0f;
    float m_PaddingLeft = 0.0f;
    float m_PaddingRight = 0.0f;
    float m_PaddingTop = 0.0f;
    float m_PaddingBottom = 0.0f;
    float m_PositionLeft = 0.0f;
    float m_PositionRight = 0.0f;
    float m_PositionTop = 0.0f;
    float m_PositionBottom = 0.0f;
    float m_Flex = 0.0f;
    float m_FlexGrow = 0.0f;
    float m_FlexShrink = 1.0f;
    float m_FlexBasis = 1.0f;
    float m_AspectRatio = 0.0f;
    uint32_t m_LayoutWidthScaleType = 0;
    uint32_t m_LayoutHeightScaleType = 0;
    uint32_t m_LayoutAlignmentType = 0;
    uint32_t m_AnimationStyleType = 0;
    uint32_t m_InterpolationType = 0;
    uint32_t m_InterpolatorId = -1;
    float m_InterpolationTime = 0.0f;
    uint32_t m_DisplayValue = 0;
    uint32_t m_PositionTypeValue = 1;
    uint32_t m_FlexDirectionValue = 2;
    uint32_t m_DirectionValue = 0;
    uint32_t m_AlignContentValue = 0;
    uint32_t m_AlignItemsValue = 1;
    uint32_t m_AlignSelfValue = 0;
    uint32_t m_JustifyContentValue = 0;
    uint32_t m_FlexWrapValue = 0;
    uint32_t m_OverflowValue = 0;
    bool m_IntrinsicallySizedValue = false;
    uint32_t m_WidthUnitsValue = 1;
    uint32_t m_HeightUnitsValue = 1;
    uint32_t m_BorderLeftUnitsValue = 0;
    uint32_t m_BorderRightUnitsValue = 0;
    uint32_t m_BorderTopUnitsValue = 0;
    uint32_t m_BorderBottomUnitsValue = 0;
    uint32_t m_MarginLeftUnitsValue = 0;
    uint32_t m_MarginRightUnitsValue = 0;
    uint32_t m_MarginTopUnitsValue = 0;
    uint32_t m_MarginBottomUnitsValue = 0;
    uint32_t m_PaddingLeftUnitsValue = 0;
    uint32_t m_PaddingRightUnitsValue = 0;
    uint32_t m_PaddingTopUnitsValue = 0;
    uint32_t m_PaddingBottomUnitsValue = 0;
    uint32_t m_PositionLeftUnitsValue = 0;
    uint32_t m_PositionRightUnitsValue = 0;
    uint32_t m_PositionTopUnitsValue = 0;
    uint32_t m_PositionBottomUnitsValue = 0;
    uint32_t m_GapHorizontalUnitsValue = 0;
    uint32_t m_GapVerticalUnitsValue = 0;
    uint32_t m_MinWidthUnitsValue = 0;
    uint32_t m_MinHeightUnitsValue = 0;
    uint32_t m_MaxWidthUnitsValue = 0;
    uint32_t m_MaxHeightUnitsValue = 0;
    bool m_LinkCornerRadius = true;
    float m_CornerRadiusTL = 0.0f;
    float m_CornerRadiusTR = 0.0f;
    float m_CornerRadiusBL = 0.0f;
    float m_CornerRadiusBR = 0.0f;

public:
    inline float gapHorizontal() const { return m_GapHorizontal; }
    void gapHorizontal(float value)
    {
        if (m_GapHorizontal == value)
        {
            return;
        }
        m_GapHorizontal = value;
        gapHorizontalChanged();
    }

    inline float gapVertical() const { return m_GapVertical; }
    void gapVertical(float value)
    {
        if (m_GapVertical == value)
        {
            return;
        }
        m_GapVertical = value;
        gapVerticalChanged();
    }

    inline float maxWidth() const { return m_MaxWidth; }
    void maxWidth(float value)
    {
        if (m_MaxWidth == value)
        {
            return;
        }
        m_MaxWidth = value;
        maxWidthChanged();
    }

    inline float maxHeight() const { return m_MaxHeight; }
    void maxHeight(float value)
    {
        if (m_MaxHeight == value)
        {
            return;
        }
        m_MaxHeight = value;
        maxHeightChanged();
    }

    inline float minWidth() const { return m_MinWidth; }
    void minWidth(float value)
    {
        if (m_MinWidth == value)
        {
            return;
        }
        m_MinWidth = value;
        minWidthChanged();
    }

    inline float minHeight() const { return m_MinHeight; }
    void minHeight(float value)
    {
        if (m_MinHeight == value)
        {
            return;
        }
        m_MinHeight = value;
        minHeightChanged();
    }

    inline float borderLeft() const { return m_BorderLeft; }
    void borderLeft(float value)
    {
        if (m_BorderLeft == value)
        {
            return;
        }
        m_BorderLeft = value;
        borderLeftChanged();
    }

    inline float borderRight() const { return m_BorderRight; }
    void borderRight(float value)
    {
        if (m_BorderRight == value)
        {
            return;
        }
        m_BorderRight = value;
        borderRightChanged();
    }

    inline float borderTop() const { return m_BorderTop; }
    void borderTop(float value)
    {
        if (m_BorderTop == value)
        {
            return;
        }
        m_BorderTop = value;
        borderTopChanged();
    }

    inline float borderBottom() const { return m_BorderBottom; }
    void borderBottom(float value)
    {
        if (m_BorderBottom == value)
        {
            return;
        }
        m_BorderBottom = value;
        borderBottomChanged();
    }

    inline float marginLeft() const { return m_MarginLeft; }
    void marginLeft(float value)
    {
        if (m_MarginLeft == value)
        {
            return;
        }
        m_MarginLeft = value;
        marginLeftChanged();
    }

    inline float marginRight() const { return m_MarginRight; }
    void marginRight(float value)
    {
        if (m_MarginRight == value)
        {
            return;
        }
        m_MarginRight = value;
        marginRightChanged();
    }

    inline float marginTop() const { return m_MarginTop; }
    void marginTop(float value)
    {
        if (m_MarginTop == value)
        {
            return;
        }
        m_MarginTop = value;
        marginTopChanged();
    }

    inline float marginBottom() const { return m_MarginBottom; }
    void marginBottom(float value)
    {
        if (m_MarginBottom == value)
        {
            return;
        }
        m_MarginBottom = value;
        marginBottomChanged();
    }

    inline float paddingLeft() const { return m_PaddingLeft; }
    void paddingLeft(float value)
    {
        if (m_PaddingLeft == value)
        {
            return;
        }
        m_PaddingLeft = value;
        paddingLeftChanged();
    }

    inline float paddingRight() const { return m_PaddingRight; }
    void paddingRight(float value)
    {
        if (m_PaddingRight == value)
        {
            return;
        }
        m_PaddingRight = value;
        paddingRightChanged();
    }

    inline float paddingTop() const { return m_PaddingTop; }
    void paddingTop(float value)
    {
        if (m_PaddingTop == value)
        {
            return;
        }
        m_PaddingTop = value;
        paddingTopChanged();
    }

    inline float paddingBottom() const { return m_PaddingBottom; }
    void paddingBottom(float value)
    {
        if (m_PaddingBottom == value)
        {
            return;
        }
        m_PaddingBottom = value;
        paddingBottomChanged();
    }

    inline float positionLeft() const { return m_PositionLeft; }
    void positionLeft(float value)
    {
        if (m_PositionLeft == value)
        {
            return;
        }
        m_PositionLeft = value;
        positionLeftChanged();
    }

    inline float positionRight() const { return m_PositionRight; }
    void positionRight(float value)
    {
        if (m_PositionRight == value)
        {
            return;
        }
        m_PositionRight = value;
        positionRightChanged();
    }

    inline float positionTop() const { return m_PositionTop; }
    void positionTop(float value)
    {
        if (m_PositionTop == value)
        {
            return;
        }
        m_PositionTop = value;
        positionTopChanged();
    }

    inline float positionBottom() const { return m_PositionBottom; }
    void positionBottom(float value)
    {
        if (m_PositionBottom == value)
        {
            return;
        }
        m_PositionBottom = value;
        positionBottomChanged();
    }

    inline float flex() const { return m_Flex; }
    void flex(float value)
    {
        if (m_Flex == value)
        {
            return;
        }
        m_Flex = value;
        flexChanged();
    }

    inline float flexGrow() const { return m_FlexGrow; }
    void flexGrow(float value)
    {
        if (m_FlexGrow == value)
        {
            return;
        }
        m_FlexGrow = value;
        flexGrowChanged();
    }

    inline float flexShrink() const { return m_FlexShrink; }
    void flexShrink(float value)
    {
        if (m_FlexShrink == value)
        {
            return;
        }
        m_FlexShrink = value;
        flexShrinkChanged();
    }

    inline float flexBasis() const { return m_FlexBasis; }
    void flexBasis(float value)
    {
        if (m_FlexBasis == value)
        {
            return;
        }
        m_FlexBasis = value;
        flexBasisChanged();
    }

    inline float aspectRatio() const { return m_AspectRatio; }
    void aspectRatio(float value)
    {
        if (m_AspectRatio == value)
        {
            return;
        }
        m_AspectRatio = value;
        aspectRatioChanged();
    }

    inline uint32_t layoutWidthScaleType() const { return m_LayoutWidthScaleType; }
    void layoutWidthScaleType(uint32_t value)
    {
        if (m_LayoutWidthScaleType == value)
        {
            return;
        }
        m_LayoutWidthScaleType = value;
        layoutWidthScaleTypeChanged();
    }

    inline uint32_t layoutHeightScaleType() const { return m_LayoutHeightScaleType; }
    void layoutHeightScaleType(uint32_t value)
    {
        if (m_LayoutHeightScaleType == value)
        {
            return;
        }
        m_LayoutHeightScaleType = value;
        layoutHeightScaleTypeChanged();
    }

    inline uint32_t layoutAlignmentType() const { return m_LayoutAlignmentType; }
    void layoutAlignmentType(uint32_t value)
    {
        if (m_LayoutAlignmentType == value)
        {
            return;
        }
        m_LayoutAlignmentType = value;
        layoutAlignmentTypeChanged();
    }

    inline uint32_t animationStyleType() const { return m_AnimationStyleType; }
    void animationStyleType(uint32_t value)
    {
        if (m_AnimationStyleType == value)
        {
            return;
        }
        m_AnimationStyleType = value;
        animationStyleTypeChanged();
    }

    inline uint32_t interpolationType() const { return m_InterpolationType; }
    void interpolationType(uint32_t value)
    {
        if (m_InterpolationType == value)
        {
            return;
        }
        m_InterpolationType = value;
        interpolationTypeChanged();
    }

    inline uint32_t interpolatorId() const { return m_InterpolatorId; }
    void interpolatorId(uint32_t value)
    {
        if (m_InterpolatorId == value)
        {
            return;
        }
        m_InterpolatorId = value;
        interpolatorIdChanged();
    }

    inline float interpolationTime() const { return m_InterpolationTime; }
    void interpolationTime(float value)
    {
        if (m_InterpolationTime == value)
        {
            return;
        }
        m_InterpolationTime = value;
        interpolationTimeChanged();
    }

    inline uint32_t displayValue() const { return m_DisplayValue; }
    void displayValue(uint32_t value)
    {
        if (m_DisplayValue == value)
        {
            return;
        }
        m_DisplayValue = value;
        displayValueChanged();
    }

    inline uint32_t positionTypeValue() const { return m_PositionTypeValue; }
    void positionTypeValue(uint32_t value)
    {
        if (m_PositionTypeValue == value)
        {
            return;
        }
        m_PositionTypeValue = value;
        positionTypeValueChanged();
    }

    inline uint32_t flexDirectionValue() const { return m_FlexDirectionValue; }
    void flexDirectionValue(uint32_t value)
    {
        if (m_FlexDirectionValue == value)
        {
            return;
        }
        m_FlexDirectionValue = value;
        flexDirectionValueChanged();
    }

    inline uint32_t directionValue() const { return m_DirectionValue; }
    void directionValue(uint32_t value)
    {
        if (m_DirectionValue == value)
        {
            return;
        }
        m_DirectionValue = value;
        directionValueChanged();
    }

    inline uint32_t alignContentValue() const { return m_AlignContentValue; }
    void alignContentValue(uint32_t value)
    {
        if (m_AlignContentValue == value)
        {
            return;
        }
        m_AlignContentValue = value;
        alignContentValueChanged();
    }

    inline uint32_t alignItemsValue() const { return m_AlignItemsValue; }
    void alignItemsValue(uint32_t value)
    {
        if (m_AlignItemsValue == value)
        {
            return;
        }
        m_AlignItemsValue = value;
        alignItemsValueChanged();
    }

    inline uint32_t alignSelfValue() const { return m_AlignSelfValue; }
    void alignSelfValue(uint32_t value)
    {
        if (m_AlignSelfValue == value)
        {
            return;
        }
        m_AlignSelfValue = value;
        alignSelfValueChanged();
    }

    inline uint32_t justifyContentValue() const { return m_JustifyContentValue; }
    void justifyContentValue(uint32_t value)
    {
        if (m_JustifyContentValue == value)
        {
            return;
        }
        m_JustifyContentValue = value;
        justifyContentValueChanged();
    }

    inline uint32_t flexWrapValue() const { return m_FlexWrapValue; }
    void flexWrapValue(uint32_t value)
    {
        if (m_FlexWrapValue == value)
        {
            return;
        }
        m_FlexWrapValue = value;
        flexWrapValueChanged();
    }

    inline uint32_t overflowValue() const { return m_OverflowValue; }
    void overflowValue(uint32_t value)
    {
        if (m_OverflowValue == value)
        {
            return;
        }
        m_OverflowValue = value;
        overflowValueChanged();
    }

    inline bool intrinsicallySizedValue() const { return m_IntrinsicallySizedValue; }
    void intrinsicallySizedValue(bool value)
    {
        if (m_IntrinsicallySizedValue == value)
        {
            return;
        }
        m_IntrinsicallySizedValue = value;
        intrinsicallySizedValueChanged();
    }

    inline uint32_t widthUnitsValue() const { return m_WidthUnitsValue; }
    void widthUnitsValue(uint32_t value)
    {
        if (m_WidthUnitsValue == value)
        {
            return;
        }
        m_WidthUnitsValue = value;
        widthUnitsValueChanged();
    }

    inline uint32_t heightUnitsValue() const { return m_HeightUnitsValue; }
    void heightUnitsValue(uint32_t value)
    {
        if (m_HeightUnitsValue == value)
        {
            return;
        }
        m_HeightUnitsValue = value;
        heightUnitsValueChanged();
    }

    inline uint32_t borderLeftUnitsValue() const { return m_BorderLeftUnitsValue; }
    void borderLeftUnitsValue(uint32_t value)
    {
        if (m_BorderLeftUnitsValue == value)
        {
            return;
        }
        m_BorderLeftUnitsValue = value;
        borderLeftUnitsValueChanged();
    }

    inline uint32_t borderRightUnitsValue() const { return m_BorderRightUnitsValue; }
    void borderRightUnitsValue(uint32_t value)
    {
        if (m_BorderRightUnitsValue == value)
        {
            return;
        }
        m_BorderRightUnitsValue = value;
        borderRightUnitsValueChanged();
    }

    inline uint32_t borderTopUnitsValue() const { return m_BorderTopUnitsValue; }
    void borderTopUnitsValue(uint32_t value)
    {
        if (m_BorderTopUnitsValue == value)
        {
            return;
        }
        m_BorderTopUnitsValue = value;
        borderTopUnitsValueChanged();
    }

    inline uint32_t borderBottomUnitsValue() const { return m_BorderBottomUnitsValue; }
    void borderBottomUnitsValue(uint32_t value)
    {
        if (m_BorderBottomUnitsValue == value)
        {
            return;
        }
        m_BorderBottomUnitsValue = value;
        borderBottomUnitsValueChanged();
    }

    inline uint32_t marginLeftUnitsValue() const { return m_MarginLeftUnitsValue; }
    void marginLeftUnitsValue(uint32_t value)
    {
        if (m_MarginLeftUnitsValue == value)
        {
            return;
        }
        m_MarginLeftUnitsValue = value;
        marginLeftUnitsValueChanged();
    }

    inline uint32_t marginRightUnitsValue() const { return m_MarginRightUnitsValue; }
    void marginRightUnitsValue(uint32_t value)
    {
        if (m_MarginRightUnitsValue == value)
        {
            return;
        }
        m_MarginRightUnitsValue = value;
        marginRightUnitsValueChanged();
    }

    inline uint32_t marginTopUnitsValue() const { return m_MarginTopUnitsValue; }
    void marginTopUnitsValue(uint32_t value)
    {
        if (m_MarginTopUnitsValue == value)
        {
            return;
        }
        m_MarginTopUnitsValue = value;
        marginTopUnitsValueChanged();
    }

    inline uint32_t marginBottomUnitsValue() const { return m_MarginBottomUnitsValue; }
    void marginBottomUnitsValue(uint32_t value)
    {
        if (m_MarginBottomUnitsValue == value)
        {
            return;
        }
        m_MarginBottomUnitsValue = value;
        marginBottomUnitsValueChanged();
    }

    inline uint32_t paddingLeftUnitsValue() const { return m_PaddingLeftUnitsValue; }
    void paddingLeftUnitsValue(uint32_t value)
    {
        if (m_PaddingLeftUnitsValue == value)
        {
            return;
        }
        m_PaddingLeftUnitsValue = value;
        paddingLeftUnitsValueChanged();
    }

    inline uint32_t paddingRightUnitsValue() const { return m_PaddingRightUnitsValue; }
    void paddingRightUnitsValue(uint32_t value)
    {
        if (m_PaddingRightUnitsValue == value)
        {
            return;
        }
        m_PaddingRightUnitsValue = value;
        paddingRightUnitsValueChanged();
    }

    inline uint32_t paddingTopUnitsValue() const { return m_PaddingTopUnitsValue; }
    void paddingTopUnitsValue(uint32_t value)
    {
        if (m_PaddingTopUnitsValue == value)
        {
            return;
        }
        m_PaddingTopUnitsValue = value;
        paddingTopUnitsValueChanged();
    }

    inline uint32_t paddingBottomUnitsValue() const { return m_PaddingBottomUnitsValue; }
    void paddingBottomUnitsValue(uint32_t value)
    {
        if (m_PaddingBottomUnitsValue == value)
        {
            return;
        }
        m_PaddingBottomUnitsValue = value;
        paddingBottomUnitsValueChanged();
    }

    inline uint32_t positionLeftUnitsValue() const { return m_PositionLeftUnitsValue; }
    void positionLeftUnitsValue(uint32_t value)
    {
        if (m_PositionLeftUnitsValue == value)
        {
            return;
        }
        m_PositionLeftUnitsValue = value;
        positionLeftUnitsValueChanged();
    }

    inline uint32_t positionRightUnitsValue() const { return m_PositionRightUnitsValue; }
    void positionRightUnitsValue(uint32_t value)
    {
        if (m_PositionRightUnitsValue == value)
        {
            return;
        }
        m_PositionRightUnitsValue = value;
        positionRightUnitsValueChanged();
    }

    inline uint32_t positionTopUnitsValue() const { return m_PositionTopUnitsValue; }
    void positionTopUnitsValue(uint32_t value)
    {
        if (m_PositionTopUnitsValue == value)
        {
            return;
        }
        m_PositionTopUnitsValue = value;
        positionTopUnitsValueChanged();
    }

    inline uint32_t positionBottomUnitsValue() const { return m_PositionBottomUnitsValue; }
    void positionBottomUnitsValue(uint32_t value)
    {
        if (m_PositionBottomUnitsValue == value)
        {
            return;
        }
        m_PositionBottomUnitsValue = value;
        positionBottomUnitsValueChanged();
    }

    inline uint32_t gapHorizontalUnitsValue() const { return m_GapHorizontalUnitsValue; }
    void gapHorizontalUnitsValue(uint32_t value)
    {
        if (m_GapHorizontalUnitsValue == value)
        {
            return;
        }
        m_GapHorizontalUnitsValue = value;
        gapHorizontalUnitsValueChanged();
    }

    inline uint32_t gapVerticalUnitsValue() const { return m_GapVerticalUnitsValue; }
    void gapVerticalUnitsValue(uint32_t value)
    {
        if (m_GapVerticalUnitsValue == value)
        {
            return;
        }
        m_GapVerticalUnitsValue = value;
        gapVerticalUnitsValueChanged();
    }

    inline uint32_t minWidthUnitsValue() const { return m_MinWidthUnitsValue; }
    void minWidthUnitsValue(uint32_t value)
    {
        if (m_MinWidthUnitsValue == value)
        {
            return;
        }
        m_MinWidthUnitsValue = value;
        minWidthUnitsValueChanged();
    }

    inline uint32_t minHeightUnitsValue() const { return m_MinHeightUnitsValue; }
    void minHeightUnitsValue(uint32_t value)
    {
        if (m_MinHeightUnitsValue == value)
        {
            return;
        }
        m_MinHeightUnitsValue = value;
        minHeightUnitsValueChanged();
    }

    inline uint32_t maxWidthUnitsValue() const { return m_MaxWidthUnitsValue; }
    void maxWidthUnitsValue(uint32_t value)
    {
        if (m_MaxWidthUnitsValue == value)
        {
            return;
        }
        m_MaxWidthUnitsValue = value;
        maxWidthUnitsValueChanged();
    }

    inline uint32_t maxHeightUnitsValue() const { return m_MaxHeightUnitsValue; }
    void maxHeightUnitsValue(uint32_t value)
    {
        if (m_MaxHeightUnitsValue == value)
        {
            return;
        }
        m_MaxHeightUnitsValue = value;
        maxHeightUnitsValueChanged();
    }

    inline bool linkCornerRadius() const { return m_LinkCornerRadius; }
    void linkCornerRadius(bool value)
    {
        if (m_LinkCornerRadius == value)
        {
            return;
        }
        m_LinkCornerRadius = value;
        linkCornerRadiusChanged();
    }

    inline float cornerRadiusTL() const { return m_CornerRadiusTL; }
    void cornerRadiusTL(float value)
    {
        if (m_CornerRadiusTL == value)
        {
            return;
        }
        m_CornerRadiusTL = value;
        cornerRadiusTLChanged();
    }

    inline float cornerRadiusTR() const { return m_CornerRadiusTR; }
    void cornerRadiusTR(float value)
    {
        if (m_CornerRadiusTR == value)
        {
            return;
        }
        m_CornerRadiusTR = value;
        cornerRadiusTRChanged();
    }

    inline float cornerRadiusBL() const { return m_CornerRadiusBL; }
    void cornerRadiusBL(float value)
    {
        if (m_CornerRadiusBL == value)
        {
            return;
        }
        m_CornerRadiusBL = value;
        cornerRadiusBLChanged();
    }

    inline float cornerRadiusBR() const { return m_CornerRadiusBR; }
    void cornerRadiusBR(float value)
    {
        if (m_CornerRadiusBR == value)
        {
            return;
        }
        m_CornerRadiusBR = value;
        cornerRadiusBRChanged();
    }

    Core* clone() const override;
    void copy(const LayoutComponentStyleBase& object)
    {
        m_GapHorizontal = object.m_GapHorizontal;
        m_GapVertical = object.m_GapVertical;
        m_MaxWidth = object.m_MaxWidth;
        m_MaxHeight = object.m_MaxHeight;
        m_MinWidth = object.m_MinWidth;
        m_MinHeight = object.m_MinHeight;
        m_BorderLeft = object.m_BorderLeft;
        m_BorderRight = object.m_BorderRight;
        m_BorderTop = object.m_BorderTop;
        m_BorderBottom = object.m_BorderBottom;
        m_MarginLeft = object.m_MarginLeft;
        m_MarginRight = object.m_MarginRight;
        m_MarginTop = object.m_MarginTop;
        m_MarginBottom = object.m_MarginBottom;
        m_PaddingLeft = object.m_PaddingLeft;
        m_PaddingRight = object.m_PaddingRight;
        m_PaddingTop = object.m_PaddingTop;
        m_PaddingBottom = object.m_PaddingBottom;
        m_PositionLeft = object.m_PositionLeft;
        m_PositionRight = object.m_PositionRight;
        m_PositionTop = object.m_PositionTop;
        m_PositionBottom = object.m_PositionBottom;
        m_Flex = object.m_Flex;
        m_FlexGrow = object.m_FlexGrow;
        m_FlexShrink = object.m_FlexShrink;
        m_FlexBasis = object.m_FlexBasis;
        m_AspectRatio = object.m_AspectRatio;
        m_LayoutWidthScaleType = object.m_LayoutWidthScaleType;
        m_LayoutHeightScaleType = object.m_LayoutHeightScaleType;
        m_LayoutAlignmentType = object.m_LayoutAlignmentType;
        m_AnimationStyleType = object.m_AnimationStyleType;
        m_InterpolationType = object.m_InterpolationType;
        m_InterpolatorId = object.m_InterpolatorId;
        m_InterpolationTime = object.m_InterpolationTime;
        m_DisplayValue = object.m_DisplayValue;
        m_PositionTypeValue = object.m_PositionTypeValue;
        m_FlexDirectionValue = object.m_FlexDirectionValue;
        m_DirectionValue = object.m_DirectionValue;
        m_AlignContentValue = object.m_AlignContentValue;
        m_AlignItemsValue = object.m_AlignItemsValue;
        m_AlignSelfValue = object.m_AlignSelfValue;
        m_JustifyContentValue = object.m_JustifyContentValue;
        m_FlexWrapValue = object.m_FlexWrapValue;
        m_OverflowValue = object.m_OverflowValue;
        m_IntrinsicallySizedValue = object.m_IntrinsicallySizedValue;
        m_WidthUnitsValue = object.m_WidthUnitsValue;
        m_HeightUnitsValue = object.m_HeightUnitsValue;
        m_BorderLeftUnitsValue = object.m_BorderLeftUnitsValue;
        m_BorderRightUnitsValue = object.m_BorderRightUnitsValue;
        m_BorderTopUnitsValue = object.m_BorderTopUnitsValue;
        m_BorderBottomUnitsValue = object.m_BorderBottomUnitsValue;
        m_MarginLeftUnitsValue = object.m_MarginLeftUnitsValue;
        m_MarginRightUnitsValue = object.m_MarginRightUnitsValue;
        m_MarginTopUnitsValue = object.m_MarginTopUnitsValue;
        m_MarginBottomUnitsValue = object.m_MarginBottomUnitsValue;
        m_PaddingLeftUnitsValue = object.m_PaddingLeftUnitsValue;
        m_PaddingRightUnitsValue = object.m_PaddingRightUnitsValue;
        m_PaddingTopUnitsValue = object.m_PaddingTopUnitsValue;
        m_PaddingBottomUnitsValue = object.m_PaddingBottomUnitsValue;
        m_PositionLeftUnitsValue = object.m_PositionLeftUnitsValue;
        m_PositionRightUnitsValue = object.m_PositionRightUnitsValue;
        m_PositionTopUnitsValue = object.m_PositionTopUnitsValue;
        m_PositionBottomUnitsValue = object.m_PositionBottomUnitsValue;
        m_GapHorizontalUnitsValue = object.m_GapHorizontalUnitsValue;
        m_GapVerticalUnitsValue = object.m_GapVerticalUnitsValue;
        m_MinWidthUnitsValue = object.m_MinWidthUnitsValue;
        m_MinHeightUnitsValue = object.m_MinHeightUnitsValue;
        m_MaxWidthUnitsValue = object.m_MaxWidthUnitsValue;
        m_MaxHeightUnitsValue = object.m_MaxHeightUnitsValue;
        m_LinkCornerRadius = object.m_LinkCornerRadius;
        m_CornerRadiusTL = object.m_CornerRadiusTL;
        m_CornerRadiusTR = object.m_CornerRadiusTR;
        m_CornerRadiusBL = object.m_CornerRadiusBL;
        m_CornerRadiusBR = object.m_CornerRadiusBR;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case gapHorizontalPropertyKey:
                m_GapHorizontal = CoreDoubleType::deserialize(reader);
                return true;
            case gapVerticalPropertyKey:
                m_GapVertical = CoreDoubleType::deserialize(reader);
                return true;
            case maxWidthPropertyKey:
                m_MaxWidth = CoreDoubleType::deserialize(reader);
                return true;
            case maxHeightPropertyKey:
                m_MaxHeight = CoreDoubleType::deserialize(reader);
                return true;
            case minWidthPropertyKey:
                m_MinWidth = CoreDoubleType::deserialize(reader);
                return true;
            case minHeightPropertyKey:
                m_MinHeight = CoreDoubleType::deserialize(reader);
                return true;
            case borderLeftPropertyKey:
                m_BorderLeft = CoreDoubleType::deserialize(reader);
                return true;
            case borderRightPropertyKey:
                m_BorderRight = CoreDoubleType::deserialize(reader);
                return true;
            case borderTopPropertyKey:
                m_BorderTop = CoreDoubleType::deserialize(reader);
                return true;
            case borderBottomPropertyKey:
                m_BorderBottom = CoreDoubleType::deserialize(reader);
                return true;
            case marginLeftPropertyKey:
                m_MarginLeft = CoreDoubleType::deserialize(reader);
                return true;
            case marginRightPropertyKey:
                m_MarginRight = CoreDoubleType::deserialize(reader);
                return true;
            case marginTopPropertyKey:
                m_MarginTop = CoreDoubleType::deserialize(reader);
                return true;
            case marginBottomPropertyKey:
                m_MarginBottom = CoreDoubleType::deserialize(reader);
                return true;
            case paddingLeftPropertyKey:
                m_PaddingLeft = CoreDoubleType::deserialize(reader);
                return true;
            case paddingRightPropertyKey:
                m_PaddingRight = CoreDoubleType::deserialize(reader);
                return true;
            case paddingTopPropertyKey:
                m_PaddingTop = CoreDoubleType::deserialize(reader);
                return true;
            case paddingBottomPropertyKey:
                m_PaddingBottom = CoreDoubleType::deserialize(reader);
                return true;
            case positionLeftPropertyKey:
                m_PositionLeft = CoreDoubleType::deserialize(reader);
                return true;
            case positionRightPropertyKey:
                m_PositionRight = CoreDoubleType::deserialize(reader);
                return true;
            case positionTopPropertyKey:
                m_PositionTop = CoreDoubleType::deserialize(reader);
                return true;
            case positionBottomPropertyKey:
                m_PositionBottom = CoreDoubleType::deserialize(reader);
                return true;
            case flexPropertyKey:
                m_Flex = CoreDoubleType::deserialize(reader);
                return true;
            case flexGrowPropertyKey:
                m_FlexGrow = CoreDoubleType::deserialize(reader);
                return true;
            case flexShrinkPropertyKey:
                m_FlexShrink = CoreDoubleType::deserialize(reader);
                return true;
            case flexBasisPropertyKey:
                m_FlexBasis = CoreDoubleType::deserialize(reader);
                return true;
            case aspectRatioPropertyKey:
                m_AspectRatio = CoreDoubleType::deserialize(reader);
                return true;
            case layoutWidthScaleTypePropertyKey:
                m_LayoutWidthScaleType = CoreUintType::deserialize(reader);
                return true;
            case layoutHeightScaleTypePropertyKey:
                m_LayoutHeightScaleType = CoreUintType::deserialize(reader);
                return true;
            case layoutAlignmentTypePropertyKey:
                m_LayoutAlignmentType = CoreUintType::deserialize(reader);
                return true;
            case animationStyleTypePropertyKey:
                m_AnimationStyleType = CoreUintType::deserialize(reader);
                return true;
            case interpolationTypePropertyKey:
                m_InterpolationType = CoreUintType::deserialize(reader);
                return true;
            case interpolatorIdPropertyKey:
                m_InterpolatorId = CoreUintType::deserialize(reader);
                return true;
            case interpolationTimePropertyKey:
                m_InterpolationTime = CoreDoubleType::deserialize(reader);
                return true;
            case displayValuePropertyKey:
                m_DisplayValue = CoreUintType::deserialize(reader);
                return true;
            case positionTypeValuePropertyKey:
                m_PositionTypeValue = CoreUintType::deserialize(reader);
                return true;
            case flexDirectionValuePropertyKey:
                m_FlexDirectionValue = CoreUintType::deserialize(reader);
                return true;
            case directionValuePropertyKey:
                m_DirectionValue = CoreUintType::deserialize(reader);
                return true;
            case alignContentValuePropertyKey:
                m_AlignContentValue = CoreUintType::deserialize(reader);
                return true;
            case alignItemsValuePropertyKey:
                m_AlignItemsValue = CoreUintType::deserialize(reader);
                return true;
            case alignSelfValuePropertyKey:
                m_AlignSelfValue = CoreUintType::deserialize(reader);
                return true;
            case justifyContentValuePropertyKey:
                m_JustifyContentValue = CoreUintType::deserialize(reader);
                return true;
            case flexWrapValuePropertyKey:
                m_FlexWrapValue = CoreUintType::deserialize(reader);
                return true;
            case overflowValuePropertyKey:
                m_OverflowValue = CoreUintType::deserialize(reader);
                return true;
            case intrinsicallySizedValuePropertyKey:
                m_IntrinsicallySizedValue = CoreBoolType::deserialize(reader);
                return true;
            case widthUnitsValuePropertyKey:
                m_WidthUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case heightUnitsValuePropertyKey:
                m_HeightUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case borderLeftUnitsValuePropertyKey:
                m_BorderLeftUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case borderRightUnitsValuePropertyKey:
                m_BorderRightUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case borderTopUnitsValuePropertyKey:
                m_BorderTopUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case borderBottomUnitsValuePropertyKey:
                m_BorderBottomUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case marginLeftUnitsValuePropertyKey:
                m_MarginLeftUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case marginRightUnitsValuePropertyKey:
                m_MarginRightUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case marginTopUnitsValuePropertyKey:
                m_MarginTopUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case marginBottomUnitsValuePropertyKey:
                m_MarginBottomUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case paddingLeftUnitsValuePropertyKey:
                m_PaddingLeftUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case paddingRightUnitsValuePropertyKey:
                m_PaddingRightUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case paddingTopUnitsValuePropertyKey:
                m_PaddingTopUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case paddingBottomUnitsValuePropertyKey:
                m_PaddingBottomUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case positionLeftUnitsValuePropertyKey:
                m_PositionLeftUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case positionRightUnitsValuePropertyKey:
                m_PositionRightUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case positionTopUnitsValuePropertyKey:
                m_PositionTopUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case positionBottomUnitsValuePropertyKey:
                m_PositionBottomUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case gapHorizontalUnitsValuePropertyKey:
                m_GapHorizontalUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case gapVerticalUnitsValuePropertyKey:
                m_GapVerticalUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case minWidthUnitsValuePropertyKey:
                m_MinWidthUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case minHeightUnitsValuePropertyKey:
                m_MinHeightUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case maxWidthUnitsValuePropertyKey:
                m_MaxWidthUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case maxHeightUnitsValuePropertyKey:
                m_MaxHeightUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case linkCornerRadiusPropertyKey:
                m_LinkCornerRadius = CoreBoolType::deserialize(reader);
                return true;
            case cornerRadiusTLPropertyKey:
                m_CornerRadiusTL = CoreDoubleType::deserialize(reader);
                return true;
            case cornerRadiusTRPropertyKey:
                m_CornerRadiusTR = CoreDoubleType::deserialize(reader);
                return true;
            case cornerRadiusBLPropertyKey:
                m_CornerRadiusBL = CoreDoubleType::deserialize(reader);
                return true;
            case cornerRadiusBRPropertyKey:
                m_CornerRadiusBR = CoreDoubleType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void gapHorizontalChanged() {}
    virtual void gapVerticalChanged() {}
    virtual void maxWidthChanged() {}
    virtual void maxHeightChanged() {}
    virtual void minWidthChanged() {}
    virtual void minHeightChanged() {}
    virtual void borderLeftChanged() {}
    virtual void borderRightChanged() {}
    virtual void borderTopChanged() {}
    virtual void borderBottomChanged() {}
    virtual void marginLeftChanged() {}
    virtual void marginRightChanged() {}
    virtual void marginTopChanged() {}
    virtual void marginBottomChanged() {}
    virtual void paddingLeftChanged() {}
    virtual void paddingRightChanged() {}
    virtual void paddingTopChanged() {}
    virtual void paddingBottomChanged() {}
    virtual void positionLeftChanged() {}
    virtual void positionRightChanged() {}
    virtual void positionTopChanged() {}
    virtual void positionBottomChanged() {}
    virtual void flexChanged() {}
    virtual void flexGrowChanged() {}
    virtual void flexShrinkChanged() {}
    virtual void flexBasisChanged() {}
    virtual void aspectRatioChanged() {}
    virtual void layoutWidthScaleTypeChanged() {}
    virtual void layoutHeightScaleTypeChanged() {}
    virtual void layoutAlignmentTypeChanged() {}
    virtual void animationStyleTypeChanged() {}
    virtual void interpolationTypeChanged() {}
    virtual void interpolatorIdChanged() {}
    virtual void interpolationTimeChanged() {}
    virtual void displayValueChanged() {}
    virtual void positionTypeValueChanged() {}
    virtual void flexDirectionValueChanged() {}
    virtual void directionValueChanged() {}
    virtual void alignContentValueChanged() {}
    virtual void alignItemsValueChanged() {}
    virtual void alignSelfValueChanged() {}
    virtual void justifyContentValueChanged() {}
    virtual void flexWrapValueChanged() {}
    virtual void overflowValueChanged() {}
    virtual void intrinsicallySizedValueChanged() {}
    virtual void widthUnitsValueChanged() {}
    virtual void heightUnitsValueChanged() {}
    virtual void borderLeftUnitsValueChanged() {}
    virtual void borderRightUnitsValueChanged() {}
    virtual void borderTopUnitsValueChanged() {}
    virtual void borderBottomUnitsValueChanged() {}
    virtual void marginLeftUnitsValueChanged() {}
    virtual void marginRightUnitsValueChanged() {}
    virtual void marginTopUnitsValueChanged() {}
    virtual void marginBottomUnitsValueChanged() {}
    virtual void paddingLeftUnitsValueChanged() {}
    virtual void paddingRightUnitsValueChanged() {}
    virtual void paddingTopUnitsValueChanged() {}
    virtual void paddingBottomUnitsValueChanged() {}
    virtual void positionLeftUnitsValueChanged() {}
    virtual void positionRightUnitsValueChanged() {}
    virtual void positionTopUnitsValueChanged() {}
    virtual void positionBottomUnitsValueChanged() {}
    virtual void gapHorizontalUnitsValueChanged() {}
    virtual void gapVerticalUnitsValueChanged() {}
    virtual void minWidthUnitsValueChanged() {}
    virtual void minHeightUnitsValueChanged() {}
    virtual void maxWidthUnitsValueChanged() {}
    virtual void maxHeightUnitsValueChanged() {}
    virtual void linkCornerRadiusChanged() {}
    virtual void cornerRadiusTLChanged() {}
    virtual void cornerRadiusTRChanged() {}
    virtual void cornerRadiusBLChanged() {}
    virtual void cornerRadiusBRChanged() {}
};
} // namespace rive

#endif