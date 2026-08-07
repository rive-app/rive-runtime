#ifndef _RIVE_LAYOUT_COMPONENT_STYLE_BASE_HPP_
#define _RIVE_LAYOUT_COMPONENT_STYLE_BASE_HPP_
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/layout/layout_sizing_style.hpp"
#include "rive/sidecar.hpp"
namespace rive
{
struct LayoutComponentStyleBorderSidecar
{
    float borderLeft = 0.0f;
    float borderRight = 0.0f;
    float borderTop = 0.0f;
    float borderBottom = 0.0f;
    uint8_t borderLeftUnitsValue = 0;
    uint8_t borderRightUnitsValue = 0;
    uint8_t borderTopUnitsValue = 0;
    uint8_t borderBottomUnitsValue = 0;
};
struct LayoutComponentStyleAbsolutePositionSidecar
{
    float positionLeft = 0.0f;
    float positionRight = 0.0f;
    float positionTop = 0.0f;
    float positionBottom = 0.0f;
    uint8_t positionLeftUnitsValue = 0;
    uint8_t positionRightUnitsValue = 0;
    uint8_t positionTopUnitsValue = 0;
    uint8_t positionBottomUnitsValue = 0;
};
struct LayoutComponentStyleCornerRadiusSidecar
{
    bool linkCornerRadius = true;
    float cornerRadiusTL = 0.0f;
    float cornerRadiusTR = 0.0f;
    float cornerRadiusBL = 0.0f;
    float cornerRadiusBR = 0.0f;
};
class LayoutComponentStyleBase : public LayoutSizingStyle
{
protected:
    typedef LayoutSizingStyle Super;

public:
    static const uint16_t typeKey = 420;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case LayoutComponentStyleBase::typeKey:
            case LayoutSizingStyleBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t gapHorizontalPropertyKey = 498;
    static const uint16_t gapVerticalPropertyKey = 499;
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
    static const uint16_t positionLeftUnitsValuePropertyKey = 621;
    static const uint16_t positionRightUnitsValuePropertyKey = 622;
    static const uint16_t positionTopUnitsValuePropertyKey = 623;
    static const uint16_t positionBottomUnitsValuePropertyKey = 624;
    static const uint16_t flexBasisPropertyKey = 523;
    static const uint16_t aspectRatioPropertyKey = 524;
    static const uint16_t interpolatorIdPropertyKey = 591;
    static const uint16_t interpolationTimePropertyKey = 592;
    static const uint16_t flexBasisUnitsValuePropertyKey = 705;
    static const uint16_t layoutAlignmentTypePropertyKey = 632;
    static const uint16_t animationStyleTypePropertyKey = 589;
    static const uint16_t interpolationTypePropertyKey = 590;
    static const uint16_t positionTypeValuePropertyKey = 597;
    static const uint16_t flexDirectionValuePropertyKey = 598;
    static const uint16_t directionValuePropertyKey = 599;
    static const uint16_t flexWrapValuePropertyKey = 604;
    static const uint16_t overflowValuePropertyKey = 605;
    static const uint16_t intrinsicallySizedValuePropertyKey = 606;
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
    static const uint16_t gapHorizontalUnitsValuePropertyKey = 625;
    static const uint16_t gapVerticalUnitsValuePropertyKey = 626;
    static const uint16_t justifyItemsValuePropertyKey = 1045;
    static const uint16_t layoutTypeValuePropertyKey = 1059;
    static const uint16_t linkCornerRadiusPropertyKey = 639;
    static const uint16_t cornerRadiusTLPropertyKey = 640;
    static const uint16_t cornerRadiusTRPropertyKey = 641;
    static const uint16_t cornerRadiusBLPropertyKey = 642;
    static const uint16_t cornerRadiusBRPropertyKey = 643;

protected:
    float m_GapHorizontal = 0.0f;
    float m_GapVertical = 0.0f;
    float m_MarginLeft = 0.0f;
    float m_MarginRight = 0.0f;
    float m_MarginTop = 0.0f;
    float m_MarginBottom = 0.0f;
    float m_PaddingLeft = 0.0f;
    float m_PaddingRight = 0.0f;
    float m_PaddingTop = 0.0f;
    float m_PaddingBottom = 0.0f;
    float m_FlexBasis = 0.0f;
    float m_AspectRatio = 0.0f;
    uint32_t m_InterpolatorId = -1;
    float m_InterpolationTime = 0.0f;
    uint8_t m_FlexBasisUnitsValue = 3;
    uint8_t m_LayoutAlignmentType = 0;
    uint8_t m_AnimationStyleType = 0;
    uint8_t m_InterpolationType = 0;
    uint8_t m_PositionTypeValue = 1;
    uint8_t m_FlexDirectionValue = 2;
    uint8_t m_DirectionValue = 0;
    uint8_t m_FlexWrapValue = 0;
    uint8_t m_OverflowValue = 0;
    bool m_IntrinsicallySizedValue = false;
    uint8_t m_MarginLeftUnitsValue = 0;
    uint8_t m_MarginRightUnitsValue = 0;
    uint8_t m_MarginTopUnitsValue = 0;
    uint8_t m_MarginBottomUnitsValue = 0;
    uint8_t m_PaddingLeftUnitsValue = 0;
    uint8_t m_PaddingRightUnitsValue = 0;
    uint8_t m_PaddingTopUnitsValue = 0;
    uint8_t m_PaddingBottomUnitsValue = 0;
    uint8_t m_GapHorizontalUnitsValue = 0;
    uint8_t m_GapVerticalUnitsValue = 0;
    uint8_t m_JustifyItemsValue = 7;
    uint8_t m_LayoutTypeValue = 0;
    Sidecar<LayoutComponentStyleBorderSidecar> m_border;
    Sidecar<LayoutComponentStyleAbsolutePositionSidecar> m_absolutePosition;
    Sidecar<LayoutComponentStyleCornerRadiusSidecar> m_cornerRadius;

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
        notifyPropertyChanged(gapHorizontalPropertyKey);
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
        notifyPropertyChanged(gapVerticalPropertyKey);
    }

    inline float borderLeft() const
    {
        auto* sidecar = m_border.get();
        return sidecar != nullptr ? sidecar->borderLeft : 0.0f;
    }
    void borderLeft(float value)
    {
        if (borderLeft() == value)
        {
            return;
        }
        m_border.ensure()->borderLeft = value;
        borderLeftChanged();
        notifyPropertyChanged(borderLeftPropertyKey);
    }

    inline float borderRight() const
    {
        auto* sidecar = m_border.get();
        return sidecar != nullptr ? sidecar->borderRight : 0.0f;
    }
    void borderRight(float value)
    {
        if (borderRight() == value)
        {
            return;
        }
        m_border.ensure()->borderRight = value;
        borderRightChanged();
        notifyPropertyChanged(borderRightPropertyKey);
    }

    inline float borderTop() const
    {
        auto* sidecar = m_border.get();
        return sidecar != nullptr ? sidecar->borderTop : 0.0f;
    }
    void borderTop(float value)
    {
        if (borderTop() == value)
        {
            return;
        }
        m_border.ensure()->borderTop = value;
        borderTopChanged();
        notifyPropertyChanged(borderTopPropertyKey);
    }

    inline float borderBottom() const
    {
        auto* sidecar = m_border.get();
        return sidecar != nullptr ? sidecar->borderBottom : 0.0f;
    }
    void borderBottom(float value)
    {
        if (borderBottom() == value)
        {
            return;
        }
        m_border.ensure()->borderBottom = value;
        borderBottomChanged();
        notifyPropertyChanged(borderBottomPropertyKey);
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
        notifyPropertyChanged(marginLeftPropertyKey);
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
        notifyPropertyChanged(marginRightPropertyKey);
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
        notifyPropertyChanged(marginTopPropertyKey);
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
        notifyPropertyChanged(marginBottomPropertyKey);
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
        notifyPropertyChanged(paddingLeftPropertyKey);
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
        notifyPropertyChanged(paddingRightPropertyKey);
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
        notifyPropertyChanged(paddingTopPropertyKey);
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
        notifyPropertyChanged(paddingBottomPropertyKey);
    }

    inline float positionLeft() const
    {
        auto* sidecar = m_absolutePosition.get();
        return sidecar != nullptr ? sidecar->positionLeft : 0.0f;
    }
    void positionLeft(float value)
    {
        if (positionLeft() == value)
        {
            return;
        }
        m_absolutePosition.ensure()->positionLeft = value;
        positionLeftChanged();
        notifyPropertyChanged(positionLeftPropertyKey);
    }

    inline float positionRight() const
    {
        auto* sidecar = m_absolutePosition.get();
        return sidecar != nullptr ? sidecar->positionRight : 0.0f;
    }
    void positionRight(float value)
    {
        if (positionRight() == value)
        {
            return;
        }
        m_absolutePosition.ensure()->positionRight = value;
        positionRightChanged();
        notifyPropertyChanged(positionRightPropertyKey);
    }

    inline float positionTop() const
    {
        auto* sidecar = m_absolutePosition.get();
        return sidecar != nullptr ? sidecar->positionTop : 0.0f;
    }
    void positionTop(float value)
    {
        if (positionTop() == value)
        {
            return;
        }
        m_absolutePosition.ensure()->positionTop = value;
        positionTopChanged();
        notifyPropertyChanged(positionTopPropertyKey);
    }

    inline float positionBottom() const
    {
        auto* sidecar = m_absolutePosition.get();
        return sidecar != nullptr ? sidecar->positionBottom : 0.0f;
    }
    void positionBottom(float value)
    {
        if (positionBottom() == value)
        {
            return;
        }
        m_absolutePosition.ensure()->positionBottom = value;
        positionBottomChanged();
        notifyPropertyChanged(positionBottomPropertyKey);
    }

    inline uint8_t positionLeftUnitsValue() const
    {
        auto* sidecar = m_absolutePosition.get();
        return sidecar != nullptr ? sidecar->positionLeftUnitsValue : 0;
    }
    void positionLeftUnitsValue(uint8_t value)
    {
        if (positionLeftUnitsValue() == value)
        {
            return;
        }
        m_absolutePosition.ensure()->positionLeftUnitsValue = value;
        positionLeftUnitsValueChanged();
        notifyPropertyChanged(positionLeftUnitsValuePropertyKey);
    }

    inline uint8_t positionRightUnitsValue() const
    {
        auto* sidecar = m_absolutePosition.get();
        return sidecar != nullptr ? sidecar->positionRightUnitsValue : 0;
    }
    void positionRightUnitsValue(uint8_t value)
    {
        if (positionRightUnitsValue() == value)
        {
            return;
        }
        m_absolutePosition.ensure()->positionRightUnitsValue = value;
        positionRightUnitsValueChanged();
        notifyPropertyChanged(positionRightUnitsValuePropertyKey);
    }

    inline uint8_t positionTopUnitsValue() const
    {
        auto* sidecar = m_absolutePosition.get();
        return sidecar != nullptr ? sidecar->positionTopUnitsValue : 0;
    }
    void positionTopUnitsValue(uint8_t value)
    {
        if (positionTopUnitsValue() == value)
        {
            return;
        }
        m_absolutePosition.ensure()->positionTopUnitsValue = value;
        positionTopUnitsValueChanged();
        notifyPropertyChanged(positionTopUnitsValuePropertyKey);
    }

    inline uint8_t positionBottomUnitsValue() const
    {
        auto* sidecar = m_absolutePosition.get();
        return sidecar != nullptr ? sidecar->positionBottomUnitsValue : 0;
    }
    void positionBottomUnitsValue(uint8_t value)
    {
        if (positionBottomUnitsValue() == value)
        {
            return;
        }
        m_absolutePosition.ensure()->positionBottomUnitsValue = value;
        positionBottomUnitsValueChanged();
        notifyPropertyChanged(positionBottomUnitsValuePropertyKey);
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
        notifyPropertyChanged(flexBasisPropertyKey);
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
        notifyPropertyChanged(aspectRatioPropertyKey);
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
        notifyPropertyChanged(interpolatorIdPropertyKey);
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
        notifyPropertyChanged(interpolationTimePropertyKey);
    }

    inline uint8_t flexBasisUnitsValue() const { return m_FlexBasisUnitsValue; }
    void flexBasisUnitsValue(uint8_t value)
    {
        if (m_FlexBasisUnitsValue == value)
        {
            return;
        }
        m_FlexBasisUnitsValue = value;
        flexBasisUnitsValueChanged();
        notifyPropertyChanged(flexBasisUnitsValuePropertyKey);
    }

    inline uint8_t layoutAlignmentType() const { return m_LayoutAlignmentType; }
    void layoutAlignmentType(uint8_t value)
    {
        if (m_LayoutAlignmentType == value)
        {
            return;
        }
        m_LayoutAlignmentType = value;
        layoutAlignmentTypeChanged();
        notifyPropertyChanged(layoutAlignmentTypePropertyKey);
    }

    inline uint8_t animationStyleType() const { return m_AnimationStyleType; }
    void animationStyleType(uint8_t value)
    {
        if (m_AnimationStyleType == value)
        {
            return;
        }
        m_AnimationStyleType = value;
        animationStyleTypeChanged();
        notifyPropertyChanged(animationStyleTypePropertyKey);
    }

    inline uint8_t interpolationType() const { return m_InterpolationType; }
    void interpolationType(uint8_t value)
    {
        if (m_InterpolationType == value)
        {
            return;
        }
        m_InterpolationType = value;
        interpolationTypeChanged();
        notifyPropertyChanged(interpolationTypePropertyKey);
    }

    inline uint8_t positionTypeValue() const { return m_PositionTypeValue; }
    void positionTypeValue(uint8_t value)
    {
        if (m_PositionTypeValue == value)
        {
            return;
        }
        m_PositionTypeValue = value;
        positionTypeValueChanged();
        notifyPropertyChanged(positionTypeValuePropertyKey);
    }

    inline uint8_t flexDirectionValue() const { return m_FlexDirectionValue; }
    void flexDirectionValue(uint8_t value)
    {
        if (m_FlexDirectionValue == value)
        {
            return;
        }
        m_FlexDirectionValue = value;
        flexDirectionValueChanged();
        notifyPropertyChanged(flexDirectionValuePropertyKey);
    }

    inline uint8_t directionValue() const { return m_DirectionValue; }
    void directionValue(uint8_t value)
    {
        if (m_DirectionValue == value)
        {
            return;
        }
        m_DirectionValue = value;
        directionValueChanged();
        notifyPropertyChanged(directionValuePropertyKey);
    }

    inline uint8_t flexWrapValue() const { return m_FlexWrapValue; }
    void flexWrapValue(uint8_t value)
    {
        if (m_FlexWrapValue == value)
        {
            return;
        }
        m_FlexWrapValue = value;
        flexWrapValueChanged();
        notifyPropertyChanged(flexWrapValuePropertyKey);
    }

    inline uint8_t overflowValue() const { return m_OverflowValue; }
    void overflowValue(uint8_t value)
    {
        if (m_OverflowValue == value)
        {
            return;
        }
        m_OverflowValue = value;
        overflowValueChanged();
        notifyPropertyChanged(overflowValuePropertyKey);
    }

    inline bool intrinsicallySizedValue() const
    {
        return m_IntrinsicallySizedValue;
    }
    void intrinsicallySizedValue(bool value)
    {
        if (m_IntrinsicallySizedValue == value)
        {
            return;
        }
        m_IntrinsicallySizedValue = value;
        intrinsicallySizedValueChanged();
        notifyPropertyChanged(intrinsicallySizedValuePropertyKey);
    }

    inline uint8_t borderLeftUnitsValue() const
    {
        auto* sidecar = m_border.get();
        return sidecar != nullptr ? sidecar->borderLeftUnitsValue : 0;
    }
    void borderLeftUnitsValue(uint8_t value)
    {
        if (borderLeftUnitsValue() == value)
        {
            return;
        }
        m_border.ensure()->borderLeftUnitsValue = value;
        borderLeftUnitsValueChanged();
        notifyPropertyChanged(borderLeftUnitsValuePropertyKey);
    }

    inline uint8_t borderRightUnitsValue() const
    {
        auto* sidecar = m_border.get();
        return sidecar != nullptr ? sidecar->borderRightUnitsValue : 0;
    }
    void borderRightUnitsValue(uint8_t value)
    {
        if (borderRightUnitsValue() == value)
        {
            return;
        }
        m_border.ensure()->borderRightUnitsValue = value;
        borderRightUnitsValueChanged();
        notifyPropertyChanged(borderRightUnitsValuePropertyKey);
    }

    inline uint8_t borderTopUnitsValue() const
    {
        auto* sidecar = m_border.get();
        return sidecar != nullptr ? sidecar->borderTopUnitsValue : 0;
    }
    void borderTopUnitsValue(uint8_t value)
    {
        if (borderTopUnitsValue() == value)
        {
            return;
        }
        m_border.ensure()->borderTopUnitsValue = value;
        borderTopUnitsValueChanged();
        notifyPropertyChanged(borderTopUnitsValuePropertyKey);
    }

    inline uint8_t borderBottomUnitsValue() const
    {
        auto* sidecar = m_border.get();
        return sidecar != nullptr ? sidecar->borderBottomUnitsValue : 0;
    }
    void borderBottomUnitsValue(uint8_t value)
    {
        if (borderBottomUnitsValue() == value)
        {
            return;
        }
        m_border.ensure()->borderBottomUnitsValue = value;
        borderBottomUnitsValueChanged();
        notifyPropertyChanged(borderBottomUnitsValuePropertyKey);
    }

    inline uint8_t marginLeftUnitsValue() const
    {
        return m_MarginLeftUnitsValue;
    }
    void marginLeftUnitsValue(uint8_t value)
    {
        if (m_MarginLeftUnitsValue == value)
        {
            return;
        }
        m_MarginLeftUnitsValue = value;
        marginLeftUnitsValueChanged();
        notifyPropertyChanged(marginLeftUnitsValuePropertyKey);
    }

    inline uint8_t marginRightUnitsValue() const
    {
        return m_MarginRightUnitsValue;
    }
    void marginRightUnitsValue(uint8_t value)
    {
        if (m_MarginRightUnitsValue == value)
        {
            return;
        }
        m_MarginRightUnitsValue = value;
        marginRightUnitsValueChanged();
        notifyPropertyChanged(marginRightUnitsValuePropertyKey);
    }

    inline uint8_t marginTopUnitsValue() const { return m_MarginTopUnitsValue; }
    void marginTopUnitsValue(uint8_t value)
    {
        if (m_MarginTopUnitsValue == value)
        {
            return;
        }
        m_MarginTopUnitsValue = value;
        marginTopUnitsValueChanged();
        notifyPropertyChanged(marginTopUnitsValuePropertyKey);
    }

    inline uint8_t marginBottomUnitsValue() const
    {
        return m_MarginBottomUnitsValue;
    }
    void marginBottomUnitsValue(uint8_t value)
    {
        if (m_MarginBottomUnitsValue == value)
        {
            return;
        }
        m_MarginBottomUnitsValue = value;
        marginBottomUnitsValueChanged();
        notifyPropertyChanged(marginBottomUnitsValuePropertyKey);
    }

    inline uint8_t paddingLeftUnitsValue() const
    {
        return m_PaddingLeftUnitsValue;
    }
    void paddingLeftUnitsValue(uint8_t value)
    {
        if (m_PaddingLeftUnitsValue == value)
        {
            return;
        }
        m_PaddingLeftUnitsValue = value;
        paddingLeftUnitsValueChanged();
        notifyPropertyChanged(paddingLeftUnitsValuePropertyKey);
    }

    inline uint8_t paddingRightUnitsValue() const
    {
        return m_PaddingRightUnitsValue;
    }
    void paddingRightUnitsValue(uint8_t value)
    {
        if (m_PaddingRightUnitsValue == value)
        {
            return;
        }
        m_PaddingRightUnitsValue = value;
        paddingRightUnitsValueChanged();
        notifyPropertyChanged(paddingRightUnitsValuePropertyKey);
    }

    inline uint8_t paddingTopUnitsValue() const
    {
        return m_PaddingTopUnitsValue;
    }
    void paddingTopUnitsValue(uint8_t value)
    {
        if (m_PaddingTopUnitsValue == value)
        {
            return;
        }
        m_PaddingTopUnitsValue = value;
        paddingTopUnitsValueChanged();
        notifyPropertyChanged(paddingTopUnitsValuePropertyKey);
    }

    inline uint8_t paddingBottomUnitsValue() const
    {
        return m_PaddingBottomUnitsValue;
    }
    void paddingBottomUnitsValue(uint8_t value)
    {
        if (m_PaddingBottomUnitsValue == value)
        {
            return;
        }
        m_PaddingBottomUnitsValue = value;
        paddingBottomUnitsValueChanged();
        notifyPropertyChanged(paddingBottomUnitsValuePropertyKey);
    }

    inline uint8_t gapHorizontalUnitsValue() const
    {
        return m_GapHorizontalUnitsValue;
    }
    void gapHorizontalUnitsValue(uint8_t value)
    {
        if (m_GapHorizontalUnitsValue == value)
        {
            return;
        }
        m_GapHorizontalUnitsValue = value;
        gapHorizontalUnitsValueChanged();
        notifyPropertyChanged(gapHorizontalUnitsValuePropertyKey);
    }

    inline uint8_t gapVerticalUnitsValue() const
    {
        return m_GapVerticalUnitsValue;
    }
    void gapVerticalUnitsValue(uint8_t value)
    {
        if (m_GapVerticalUnitsValue == value)
        {
            return;
        }
        m_GapVerticalUnitsValue = value;
        gapVerticalUnitsValueChanged();
        notifyPropertyChanged(gapVerticalUnitsValuePropertyKey);
    }

    inline uint8_t justifyItemsValue() const { return m_JustifyItemsValue; }
    void justifyItemsValue(uint8_t value)
    {
        if (m_JustifyItemsValue == value)
        {
            return;
        }
        m_JustifyItemsValue = value;
        justifyItemsValueChanged();
        notifyPropertyChanged(justifyItemsValuePropertyKey);
    }

    inline uint8_t layoutTypeValue() const { return m_LayoutTypeValue; }
    void layoutTypeValue(uint8_t value)
    {
        if (m_LayoutTypeValue == value)
        {
            return;
        }
        m_LayoutTypeValue = value;
        layoutTypeValueChanged();
        notifyPropertyChanged(layoutTypeValuePropertyKey);
    }

    inline bool linkCornerRadius() const
    {
        auto* sidecar = m_cornerRadius.get();
        return sidecar != nullptr ? sidecar->linkCornerRadius : true;
    }
    void linkCornerRadius(bool value)
    {
        if (linkCornerRadius() == value)
        {
            return;
        }
        m_cornerRadius.ensure()->linkCornerRadius = value;
        linkCornerRadiusChanged();
        notifyPropertyChanged(linkCornerRadiusPropertyKey);
    }

    inline float cornerRadiusTL() const
    {
        auto* sidecar = m_cornerRadius.get();
        return sidecar != nullptr ? sidecar->cornerRadiusTL : 0.0f;
    }
    void cornerRadiusTL(float value)
    {
        if (cornerRadiusTL() == value)
        {
            return;
        }
        m_cornerRadius.ensure()->cornerRadiusTL = value;
        cornerRadiusTLChanged();
        notifyPropertyChanged(cornerRadiusTLPropertyKey);
    }

    inline float cornerRadiusTR() const
    {
        auto* sidecar = m_cornerRadius.get();
        return sidecar != nullptr ? sidecar->cornerRadiusTR : 0.0f;
    }
    void cornerRadiusTR(float value)
    {
        if (cornerRadiusTR() == value)
        {
            return;
        }
        m_cornerRadius.ensure()->cornerRadiusTR = value;
        cornerRadiusTRChanged();
        notifyPropertyChanged(cornerRadiusTRPropertyKey);
    }

    inline float cornerRadiusBL() const
    {
        auto* sidecar = m_cornerRadius.get();
        return sidecar != nullptr ? sidecar->cornerRadiusBL : 0.0f;
    }
    void cornerRadiusBL(float value)
    {
        if (cornerRadiusBL() == value)
        {
            return;
        }
        m_cornerRadius.ensure()->cornerRadiusBL = value;
        cornerRadiusBLChanged();
        notifyPropertyChanged(cornerRadiusBLPropertyKey);
    }

    inline float cornerRadiusBR() const
    {
        auto* sidecar = m_cornerRadius.get();
        return sidecar != nullptr ? sidecar->cornerRadiusBR : 0.0f;
    }
    void cornerRadiusBR(float value)
    {
        if (cornerRadiusBR() == value)
        {
            return;
        }
        m_cornerRadius.ensure()->cornerRadiusBR = value;
        cornerRadiusBRChanged();
        notifyPropertyChanged(cornerRadiusBRPropertyKey);
    }

    Core* clone() const override;
    void copy(const LayoutComponentStyleBase& object)
    {
        m_GapHorizontal = object.m_GapHorizontal;
        m_GapVertical = object.m_GapVertical;
        m_MarginLeft = object.m_MarginLeft;
        m_MarginRight = object.m_MarginRight;
        m_MarginTop = object.m_MarginTop;
        m_MarginBottom = object.m_MarginBottom;
        m_PaddingLeft = object.m_PaddingLeft;
        m_PaddingRight = object.m_PaddingRight;
        m_PaddingTop = object.m_PaddingTop;
        m_PaddingBottom = object.m_PaddingBottom;
        m_FlexBasis = object.m_FlexBasis;
        m_AspectRatio = object.m_AspectRatio;
        m_InterpolatorId = object.m_InterpolatorId;
        m_InterpolationTime = object.m_InterpolationTime;
        m_FlexBasisUnitsValue = object.m_FlexBasisUnitsValue;
        m_LayoutAlignmentType = object.m_LayoutAlignmentType;
        m_AnimationStyleType = object.m_AnimationStyleType;
        m_InterpolationType = object.m_InterpolationType;
        m_PositionTypeValue = object.m_PositionTypeValue;
        m_FlexDirectionValue = object.m_FlexDirectionValue;
        m_DirectionValue = object.m_DirectionValue;
        m_FlexWrapValue = object.m_FlexWrapValue;
        m_OverflowValue = object.m_OverflowValue;
        m_IntrinsicallySizedValue = object.m_IntrinsicallySizedValue;
        m_MarginLeftUnitsValue = object.m_MarginLeftUnitsValue;
        m_MarginRightUnitsValue = object.m_MarginRightUnitsValue;
        m_MarginTopUnitsValue = object.m_MarginTopUnitsValue;
        m_MarginBottomUnitsValue = object.m_MarginBottomUnitsValue;
        m_PaddingLeftUnitsValue = object.m_PaddingLeftUnitsValue;
        m_PaddingRightUnitsValue = object.m_PaddingRightUnitsValue;
        m_PaddingTopUnitsValue = object.m_PaddingTopUnitsValue;
        m_PaddingBottomUnitsValue = object.m_PaddingBottomUnitsValue;
        m_GapHorizontalUnitsValue = object.m_GapHorizontalUnitsValue;
        m_GapVerticalUnitsValue = object.m_GapVerticalUnitsValue;
        m_JustifyItemsValue = object.m_JustifyItemsValue;
        m_LayoutTypeValue = object.m_LayoutTypeValue;
        m_border = object.m_border;
        m_absolutePosition = object.m_absolutePosition;
        m_cornerRadius = object.m_cornerRadius;
        LayoutSizingStyle::copy(object);
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
            case borderLeftPropertyKey:
                m_border.ensure()->borderLeft =
                    CoreDoubleType::deserialize(reader);
                return true;
            case borderRightPropertyKey:
                m_border.ensure()->borderRight =
                    CoreDoubleType::deserialize(reader);
                return true;
            case borderTopPropertyKey:
                m_border.ensure()->borderTop =
                    CoreDoubleType::deserialize(reader);
                return true;
            case borderBottomPropertyKey:
                m_border.ensure()->borderBottom =
                    CoreDoubleType::deserialize(reader);
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
                m_absolutePosition.ensure()->positionLeft =
                    CoreDoubleType::deserialize(reader);
                return true;
            case positionRightPropertyKey:
                m_absolutePosition.ensure()->positionRight =
                    CoreDoubleType::deserialize(reader);
                return true;
            case positionTopPropertyKey:
                m_absolutePosition.ensure()->positionTop =
                    CoreDoubleType::deserialize(reader);
                return true;
            case positionBottomPropertyKey:
                m_absolutePosition.ensure()->positionBottom =
                    CoreDoubleType::deserialize(reader);
                return true;
            case positionLeftUnitsValuePropertyKey:
                m_absolutePosition.ensure()->positionLeftUnitsValue =
                    CoreUintType::deserialize(reader);
                return true;
            case positionRightUnitsValuePropertyKey:
                m_absolutePosition.ensure()->positionRightUnitsValue =
                    CoreUintType::deserialize(reader);
                return true;
            case positionTopUnitsValuePropertyKey:
                m_absolutePosition.ensure()->positionTopUnitsValue =
                    CoreUintType::deserialize(reader);
                return true;
            case positionBottomUnitsValuePropertyKey:
                m_absolutePosition.ensure()->positionBottomUnitsValue =
                    CoreUintType::deserialize(reader);
                return true;
            case flexBasisPropertyKey:
                m_FlexBasis = CoreDoubleType::deserialize(reader);
                return true;
            case aspectRatioPropertyKey:
                m_AspectRatio = CoreDoubleType::deserialize(reader);
                return true;
            case interpolatorIdPropertyKey:
                m_InterpolatorId = CoreUintType::deserialize(reader);
                return true;
            case interpolationTimePropertyKey:
                m_InterpolationTime = CoreDoubleType::deserialize(reader);
                return true;
            case flexBasisUnitsValuePropertyKey:
                m_FlexBasisUnitsValue = CoreUintType::deserialize(reader);
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
            case positionTypeValuePropertyKey:
                m_PositionTypeValue = CoreUintType::deserialize(reader);
                return true;
            case flexDirectionValuePropertyKey:
                m_FlexDirectionValue = CoreUintType::deserialize(reader);
                return true;
            case directionValuePropertyKey:
                m_DirectionValue = CoreUintType::deserialize(reader);
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
            case borderLeftUnitsValuePropertyKey:
                m_border.ensure()->borderLeftUnitsValue =
                    CoreUintType::deserialize(reader);
                return true;
            case borderRightUnitsValuePropertyKey:
                m_border.ensure()->borderRightUnitsValue =
                    CoreUintType::deserialize(reader);
                return true;
            case borderTopUnitsValuePropertyKey:
                m_border.ensure()->borderTopUnitsValue =
                    CoreUintType::deserialize(reader);
                return true;
            case borderBottomUnitsValuePropertyKey:
                m_border.ensure()->borderBottomUnitsValue =
                    CoreUintType::deserialize(reader);
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
            case gapHorizontalUnitsValuePropertyKey:
                m_GapHorizontalUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case gapVerticalUnitsValuePropertyKey:
                m_GapVerticalUnitsValue = CoreUintType::deserialize(reader);
                return true;
            case justifyItemsValuePropertyKey:
                m_JustifyItemsValue = CoreUintType::deserialize(reader);
                return true;
            case layoutTypeValuePropertyKey:
                m_LayoutTypeValue = CoreUintType::deserialize(reader);
                return true;
            case linkCornerRadiusPropertyKey:
                m_cornerRadius.ensure()->linkCornerRadius =
                    CoreBoolType::deserialize(reader);
                return true;
            case cornerRadiusTLPropertyKey:
                m_cornerRadius.ensure()->cornerRadiusTL =
                    CoreDoubleType::deserialize(reader);
                return true;
            case cornerRadiusTRPropertyKey:
                m_cornerRadius.ensure()->cornerRadiusTR =
                    CoreDoubleType::deserialize(reader);
                return true;
            case cornerRadiusBLPropertyKey:
                m_cornerRadius.ensure()->cornerRadiusBL =
                    CoreDoubleType::deserialize(reader);
                return true;
            case cornerRadiusBRPropertyKey:
                m_cornerRadius.ensure()->cornerRadiusBR =
                    CoreDoubleType::deserialize(reader);
                return true;
        }
        return LayoutSizingStyle::deserialize(propertyKey, reader);
    }

protected:
    virtual void gapHorizontalChanged() {}
    virtual void gapVerticalChanged() {}
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
    virtual void positionLeftUnitsValueChanged() {}
    virtual void positionRightUnitsValueChanged() {}
    virtual void positionTopUnitsValueChanged() {}
    virtual void positionBottomUnitsValueChanged() {}
    virtual void flexBasisChanged() {}
    virtual void aspectRatioChanged() {}
    virtual void interpolatorIdChanged() {}
    virtual void interpolationTimeChanged() {}
    virtual void flexBasisUnitsValueChanged() {}
    virtual void layoutAlignmentTypeChanged() {}
    virtual void animationStyleTypeChanged() {}
    virtual void interpolationTypeChanged() {}
    virtual void positionTypeValueChanged() {}
    virtual void flexDirectionValueChanged() {}
    virtual void directionValueChanged() {}
    virtual void flexWrapValueChanged() {}
    virtual void overflowValueChanged() {}
    virtual void intrinsicallySizedValueChanged() {}
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
    virtual void gapHorizontalUnitsValueChanged() {}
    virtual void gapVerticalUnitsValueChanged() {}
    virtual void justifyItemsValueChanged() {}
    virtual void layoutTypeValueChanged() {}
    virtual void linkCornerRadiusChanged() {}
    virtual void cornerRadiusTLChanged() {}
    virtual void cornerRadiusTRChanged() {}
    virtual void cornerRadiusBLChanged() {}
    virtual void cornerRadiusBRChanged() {}
};
} // namespace rive

#endif