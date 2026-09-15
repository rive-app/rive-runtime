#ifndef _RIVE_LAYOUT_COMPONENT_STYLE_BASE_HPP_
#define _RIVE_LAYOUT_COMPONENT_STYLE_BASE_HPP_
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
#include "rive/layout/layout_sizing_style.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
#ifndef WITH_RIVE_EDITOR
#include "rive/sidecar.hpp"
#endif
namespace rive
{
#ifndef WITH_RIVE_EDITOR
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
#endif
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
#ifdef WITH_RIVE_EDITOR
    float m_BorderLeft = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_BorderRight = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_BorderTop = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_BorderBottom = 0.0f;
#endif
    float m_MarginLeft = 0.0f;
    float m_MarginRight = 0.0f;
    float m_MarginTop = 0.0f;
    float m_MarginBottom = 0.0f;
    float m_PaddingLeft = 0.0f;
    float m_PaddingRight = 0.0f;
    float m_PaddingTop = 0.0f;
    float m_PaddingBottom = 0.0f;
#ifdef WITH_RIVE_EDITOR
    float m_PositionLeft = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_PositionRight = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_PositionTop = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_PositionBottom = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    uint8_t m_PositionLeftUnitsValue = 0;
#endif
#ifdef WITH_RIVE_EDITOR
    uint8_t m_PositionRightUnitsValue = 0;
#endif
#ifdef WITH_RIVE_EDITOR
    uint8_t m_PositionTopUnitsValue = 0;
#endif
#ifdef WITH_RIVE_EDITOR
    uint8_t m_PositionBottomUnitsValue = 0;
#endif
    float m_FlexBasis = 0.0f;
    float m_AspectRatio = 0.0f;
    Id m_InterpolatorId = kEmptyId;
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
#ifdef WITH_RIVE_EDITOR
    uint8_t m_BorderLeftUnitsValue = 0;
#endif
#ifdef WITH_RIVE_EDITOR
    uint8_t m_BorderRightUnitsValue = 0;
#endif
#ifdef WITH_RIVE_EDITOR
    uint8_t m_BorderTopUnitsValue = 0;
#endif
#ifdef WITH_RIVE_EDITOR
    uint8_t m_BorderBottomUnitsValue = 0;
#endif
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
#ifdef WITH_RIVE_EDITOR
    bool m_LinkCornerRadius = true;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_CornerRadiusTL = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_CornerRadiusTR = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_CornerRadiusBL = 0.0f;
#endif
#ifdef WITH_RIVE_EDITOR
    float m_CornerRadiusBR = 0.0f;
#endif
#ifndef WITH_RIVE_EDITOR
    Sidecar<LayoutComponentStyleBorderSidecar> m_border;
    Sidecar<LayoutComponentStyleAbsolutePositionSidecar> m_absolutePosition;
    Sidecar<LayoutComponentStyleCornerRadiusSidecar> m_cornerRadius;
#endif
public:
    inline float gapHorizontal() const { return m_GapHorizontal; }
    void gapHorizontal(float value)
    {
        if (m_GapHorizontal == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(gapHorizontalPropertyKey,
                             &m_GapHorizontal,
                             &value);
        m_GapHorizontal = value;
        RIVE_EDITOR_CHANGED(gapHorizontalChanged());
        notifyPropertyChanged(gapHorizontalPropertyKey);
    }

    inline float gapVertical() const { return m_GapVertical; }
    void gapVertical(float value)
    {
        if (m_GapVertical == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(gapVerticalPropertyKey, &m_GapVertical, &value);
        m_GapVertical = value;
        RIVE_EDITOR_CHANGED(gapVerticalChanged());
        notifyPropertyChanged(gapVerticalPropertyKey);
    }

#ifdef WITH_RIVE_EDITOR
    inline float borderLeft() const { return m_BorderLeft; }
    void borderLeft(float value)
    {
        if (m_BorderLeft == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(borderLeftPropertyKey, &m_BorderLeft, &value);
        m_BorderLeft = value;
        RIVE_EDITOR_CHANGED(borderLeftChanged());
        notifyPropertyChanged(borderLeftPropertyKey);
    }
#else
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
        m_border.ensureAllocated()->borderLeft = value;
        borderLeftChanged();
        notifyPropertyChanged(borderLeftPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline float borderRight() const { return m_BorderRight; }
    void borderRight(float value)
    {
        if (m_BorderRight == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(borderRightPropertyKey, &m_BorderRight, &value);
        m_BorderRight = value;
        RIVE_EDITOR_CHANGED(borderRightChanged());
        notifyPropertyChanged(borderRightPropertyKey);
    }
#else
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
        m_border.ensureAllocated()->borderRight = value;
        borderRightChanged();
        notifyPropertyChanged(borderRightPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline float borderTop() const { return m_BorderTop; }
    void borderTop(float value)
    {
        if (m_BorderTop == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(borderTopPropertyKey, &m_BorderTop, &value);
        m_BorderTop = value;
        RIVE_EDITOR_CHANGED(borderTopChanged());
        notifyPropertyChanged(borderTopPropertyKey);
    }
#else
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
        m_border.ensureAllocated()->borderTop = value;
        borderTopChanged();
        notifyPropertyChanged(borderTopPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline float borderBottom() const { return m_BorderBottom; }
    void borderBottom(float value)
    {
        if (m_BorderBottom == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(borderBottomPropertyKey, &m_BorderBottom, &value);
        m_BorderBottom = value;
        RIVE_EDITOR_CHANGED(borderBottomChanged());
        notifyPropertyChanged(borderBottomPropertyKey);
    }
#else
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
        m_border.ensureAllocated()->borderBottom = value;
        borderBottomChanged();
        notifyPropertyChanged(borderBottomPropertyKey);
    }
#endif

    inline float marginLeft() const { return m_MarginLeft; }
    void marginLeft(float value)
    {
        if (m_MarginLeft == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(marginLeftPropertyKey, &m_MarginLeft, &value);
        m_MarginLeft = value;
        RIVE_EDITOR_CHANGED(marginLeftChanged());
        notifyPropertyChanged(marginLeftPropertyKey);
    }

    inline float marginRight() const { return m_MarginRight; }
    void marginRight(float value)
    {
        if (m_MarginRight == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(marginRightPropertyKey, &m_MarginRight, &value);
        m_MarginRight = value;
        RIVE_EDITOR_CHANGED(marginRightChanged());
        notifyPropertyChanged(marginRightPropertyKey);
    }

    inline float marginTop() const { return m_MarginTop; }
    void marginTop(float value)
    {
        if (m_MarginTop == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(marginTopPropertyKey, &m_MarginTop, &value);
        m_MarginTop = value;
        RIVE_EDITOR_CHANGED(marginTopChanged());
        notifyPropertyChanged(marginTopPropertyKey);
    }

    inline float marginBottom() const { return m_MarginBottom; }
    void marginBottom(float value)
    {
        if (m_MarginBottom == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(marginBottomPropertyKey, &m_MarginBottom, &value);
        m_MarginBottom = value;
        RIVE_EDITOR_CHANGED(marginBottomChanged());
        notifyPropertyChanged(marginBottomPropertyKey);
    }

    inline float paddingLeft() const { return m_PaddingLeft; }
    void paddingLeft(float value)
    {
        if (m_PaddingLeft == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(paddingLeftPropertyKey, &m_PaddingLeft, &value);
        m_PaddingLeft = value;
        RIVE_EDITOR_CHANGED(paddingLeftChanged());
        notifyPropertyChanged(paddingLeftPropertyKey);
    }

    inline float paddingRight() const { return m_PaddingRight; }
    void paddingRight(float value)
    {
        if (m_PaddingRight == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(paddingRightPropertyKey, &m_PaddingRight, &value);
        m_PaddingRight = value;
        RIVE_EDITOR_CHANGED(paddingRightChanged());
        notifyPropertyChanged(paddingRightPropertyKey);
    }

    inline float paddingTop() const { return m_PaddingTop; }
    void paddingTop(float value)
    {
        if (m_PaddingTop == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(paddingTopPropertyKey, &m_PaddingTop, &value);
        m_PaddingTop = value;
        RIVE_EDITOR_CHANGED(paddingTopChanged());
        notifyPropertyChanged(paddingTopPropertyKey);
    }

    inline float paddingBottom() const { return m_PaddingBottom; }
    void paddingBottom(float value)
    {
        if (m_PaddingBottom == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(paddingBottomPropertyKey,
                             &m_PaddingBottom,
                             &value);
        m_PaddingBottom = value;
        RIVE_EDITOR_CHANGED(paddingBottomChanged());
        notifyPropertyChanged(paddingBottomPropertyKey);
    }

#ifdef WITH_RIVE_EDITOR
    inline float positionLeft() const { return m_PositionLeft; }
    void positionLeft(float value)
    {
        if (m_PositionLeft == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(positionLeftPropertyKey, &m_PositionLeft, &value);
        m_PositionLeft = value;
        RIVE_EDITOR_CHANGED(positionLeftChanged());
        notifyPropertyChanged(positionLeftPropertyKey);
    }
#else
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
        m_absolutePosition.ensureAllocated()->positionLeft = value;
        positionLeftChanged();
        notifyPropertyChanged(positionLeftPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline float positionRight() const { return m_PositionRight; }
    void positionRight(float value)
    {
        if (m_PositionRight == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(positionRightPropertyKey,
                             &m_PositionRight,
                             &value);
        m_PositionRight = value;
        RIVE_EDITOR_CHANGED(positionRightChanged());
        notifyPropertyChanged(positionRightPropertyKey);
    }
#else
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
        m_absolutePosition.ensureAllocated()->positionRight = value;
        positionRightChanged();
        notifyPropertyChanged(positionRightPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline float positionTop() const { return m_PositionTop; }
    void positionTop(float value)
    {
        if (m_PositionTop == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(positionTopPropertyKey, &m_PositionTop, &value);
        m_PositionTop = value;
        RIVE_EDITOR_CHANGED(positionTopChanged());
        notifyPropertyChanged(positionTopPropertyKey);
    }
#else
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
        m_absolutePosition.ensureAllocated()->positionTop = value;
        positionTopChanged();
        notifyPropertyChanged(positionTopPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline float positionBottom() const { return m_PositionBottom; }
    void positionBottom(float value)
    {
        if (m_PositionBottom == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(positionBottomPropertyKey,
                             &m_PositionBottom,
                             &value);
        m_PositionBottom = value;
        RIVE_EDITOR_CHANGED(positionBottomChanged());
        notifyPropertyChanged(positionBottomPropertyKey);
    }
#else
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
        m_absolutePosition.ensureAllocated()->positionBottom = value;
        positionBottomChanged();
        notifyPropertyChanged(positionBottomPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline uint8_t positionLeftUnitsValue() const
    {
        return m_PositionLeftUnitsValue;
    }
    void positionLeftUnitsValue(uint8_t value)
    {
        if (m_PositionLeftUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(positionLeftUnitsValuePropertyKey,
                             &m_PositionLeftUnitsValue,
                             &value);
        m_PositionLeftUnitsValue = value;
        RIVE_EDITOR_CHANGED(positionLeftUnitsValueChanged());
        notifyPropertyChanged(positionLeftUnitsValuePropertyKey);
    }
#else
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
        m_absolutePosition.ensureAllocated()->positionLeftUnitsValue = value;
        positionLeftUnitsValueChanged();
        notifyPropertyChanged(positionLeftUnitsValuePropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline uint8_t positionRightUnitsValue() const
    {
        return m_PositionRightUnitsValue;
    }
    void positionRightUnitsValue(uint8_t value)
    {
        if (m_PositionRightUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(positionRightUnitsValuePropertyKey,
                             &m_PositionRightUnitsValue,
                             &value);
        m_PositionRightUnitsValue = value;
        RIVE_EDITOR_CHANGED(positionRightUnitsValueChanged());
        notifyPropertyChanged(positionRightUnitsValuePropertyKey);
    }
#else
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
        m_absolutePosition.ensureAllocated()->positionRightUnitsValue = value;
        positionRightUnitsValueChanged();
        notifyPropertyChanged(positionRightUnitsValuePropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline uint8_t positionTopUnitsValue() const
    {
        return m_PositionTopUnitsValue;
    }
    void positionTopUnitsValue(uint8_t value)
    {
        if (m_PositionTopUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(positionTopUnitsValuePropertyKey,
                             &m_PositionTopUnitsValue,
                             &value);
        m_PositionTopUnitsValue = value;
        RIVE_EDITOR_CHANGED(positionTopUnitsValueChanged());
        notifyPropertyChanged(positionTopUnitsValuePropertyKey);
    }
#else
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
        m_absolutePosition.ensureAllocated()->positionTopUnitsValue = value;
        positionTopUnitsValueChanged();
        notifyPropertyChanged(positionTopUnitsValuePropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline uint8_t positionBottomUnitsValue() const
    {
        return m_PositionBottomUnitsValue;
    }
    void positionBottomUnitsValue(uint8_t value)
    {
        if (m_PositionBottomUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(positionBottomUnitsValuePropertyKey,
                             &m_PositionBottomUnitsValue,
                             &value);
        m_PositionBottomUnitsValue = value;
        RIVE_EDITOR_CHANGED(positionBottomUnitsValueChanged());
        notifyPropertyChanged(positionBottomUnitsValuePropertyKey);
    }
#else
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
        m_absolutePosition.ensureAllocated()->positionBottomUnitsValue = value;
        positionBottomUnitsValueChanged();
        notifyPropertyChanged(positionBottomUnitsValuePropertyKey);
    }
#endif

    inline float flexBasis() const { return m_FlexBasis; }
    void flexBasis(float value)
    {
        if (m_FlexBasis == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(flexBasisPropertyKey, &m_FlexBasis, &value);
        m_FlexBasis = value;
        RIVE_EDITOR_CHANGED(flexBasisChanged());
        notifyPropertyChanged(flexBasisPropertyKey);
    }

    inline float aspectRatio() const { return m_AspectRatio; }
    void aspectRatio(float value)
    {
        if (m_AspectRatio == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(aspectRatioPropertyKey, &m_AspectRatio, &value);
        m_AspectRatio = value;
        RIVE_EDITOR_CHANGED(aspectRatioChanged());
        notifyPropertyChanged(aspectRatioPropertyKey);
    }

    inline Id interpolatorId() const { return m_InterpolatorId; }
    void interpolatorId(Id value)
    {
        if (m_InterpolatorId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(interpolatorIdPropertyKey,
                             &m_InterpolatorId,
                             &value);
        m_InterpolatorId = value;
        RIVE_EDITOR_CHANGED(interpolatorIdChanged());
        notifyPropertyChanged(interpolatorIdPropertyKey);
    }

    inline float interpolationTime() const { return m_InterpolationTime; }
    void interpolationTime(float value)
    {
        if (m_InterpolationTime == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(interpolationTimePropertyKey,
                             &m_InterpolationTime,
                             &value);
        m_InterpolationTime = value;
        RIVE_EDITOR_CHANGED(interpolationTimeChanged());
        notifyPropertyChanged(interpolationTimePropertyKey);
    }

    inline uint8_t flexBasisUnitsValue() const { return m_FlexBasisUnitsValue; }
    void flexBasisUnitsValue(uint8_t value)
    {
        if (m_FlexBasisUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(flexBasisUnitsValuePropertyKey,
                             &m_FlexBasisUnitsValue,
                             &value);
        m_FlexBasisUnitsValue = value;
        RIVE_EDITOR_CHANGED(flexBasisUnitsValueChanged());
        notifyPropertyChanged(flexBasisUnitsValuePropertyKey);
    }

    inline uint8_t layoutAlignmentType() const { return m_LayoutAlignmentType; }
    void layoutAlignmentType(uint8_t value)
    {
        if (m_LayoutAlignmentType == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(layoutAlignmentTypePropertyKey,
                             &m_LayoutAlignmentType,
                             &value);
        m_LayoutAlignmentType = value;
        RIVE_EDITOR_CHANGED(layoutAlignmentTypeChanged());
        notifyPropertyChanged(layoutAlignmentTypePropertyKey);
    }

    inline uint8_t animationStyleType() const { return m_AnimationStyleType; }
    void animationStyleType(uint8_t value)
    {
        if (m_AnimationStyleType == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(animationStyleTypePropertyKey,
                             &m_AnimationStyleType,
                             &value);
        m_AnimationStyleType = value;
        RIVE_EDITOR_CHANGED(animationStyleTypeChanged());
        notifyPropertyChanged(animationStyleTypePropertyKey);
    }

    inline uint8_t interpolationType() const { return m_InterpolationType; }
    void interpolationType(uint8_t value)
    {
        if (m_InterpolationType == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(interpolationTypePropertyKey,
                             &m_InterpolationType,
                             &value);
        m_InterpolationType = value;
        RIVE_EDITOR_CHANGED(interpolationTypeChanged());
        notifyPropertyChanged(interpolationTypePropertyKey);
    }

    inline uint8_t positionTypeValue() const { return m_PositionTypeValue; }
    void positionTypeValue(uint8_t value)
    {
        if (m_PositionTypeValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(positionTypeValuePropertyKey,
                             &m_PositionTypeValue,
                             &value);
        m_PositionTypeValue = value;
        RIVE_EDITOR_CHANGED(positionTypeValueChanged());
        notifyPropertyChanged(positionTypeValuePropertyKey);
    }

    inline uint8_t flexDirectionValue() const { return m_FlexDirectionValue; }
    void flexDirectionValue(uint8_t value)
    {
        if (m_FlexDirectionValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(flexDirectionValuePropertyKey,
                             &m_FlexDirectionValue,
                             &value);
        m_FlexDirectionValue = value;
        RIVE_EDITOR_CHANGED(flexDirectionValueChanged());
        notifyPropertyChanged(flexDirectionValuePropertyKey);
    }

    inline uint8_t directionValue() const { return m_DirectionValue; }
    void directionValue(uint8_t value)
    {
        if (m_DirectionValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(directionValuePropertyKey,
                             &m_DirectionValue,
                             &value);
        m_DirectionValue = value;
        RIVE_EDITOR_CHANGED(directionValueChanged());
        notifyPropertyChanged(directionValuePropertyKey);
    }

    inline uint8_t flexWrapValue() const { return m_FlexWrapValue; }
    void flexWrapValue(uint8_t value)
    {
        if (m_FlexWrapValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(flexWrapValuePropertyKey,
                             &m_FlexWrapValue,
                             &value);
        m_FlexWrapValue = value;
        RIVE_EDITOR_CHANGED(flexWrapValueChanged());
        notifyPropertyChanged(flexWrapValuePropertyKey);
    }

    inline uint8_t overflowValue() const { return m_OverflowValue; }
    void overflowValue(uint8_t value)
    {
        if (m_OverflowValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(overflowValuePropertyKey,
                             &m_OverflowValue,
                             &value);
        m_OverflowValue = value;
        RIVE_EDITOR_CHANGED(overflowValueChanged());
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
        RIVE_EDITOR_CHANGING(intrinsicallySizedValuePropertyKey,
                             &m_IntrinsicallySizedValue,
                             &value);
        m_IntrinsicallySizedValue = value;
        RIVE_EDITOR_CHANGED(intrinsicallySizedValueChanged());
        notifyPropertyChanged(intrinsicallySizedValuePropertyKey);
    }

#ifdef WITH_RIVE_EDITOR
    inline uint8_t borderLeftUnitsValue() const
    {
        return m_BorderLeftUnitsValue;
    }
    void borderLeftUnitsValue(uint8_t value)
    {
        if (m_BorderLeftUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(borderLeftUnitsValuePropertyKey,
                             &m_BorderLeftUnitsValue,
                             &value);
        m_BorderLeftUnitsValue = value;
        RIVE_EDITOR_CHANGED(borderLeftUnitsValueChanged());
        notifyPropertyChanged(borderLeftUnitsValuePropertyKey);
    }
#else
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
        m_border.ensureAllocated()->borderLeftUnitsValue = value;
        borderLeftUnitsValueChanged();
        notifyPropertyChanged(borderLeftUnitsValuePropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline uint8_t borderRightUnitsValue() const
    {
        return m_BorderRightUnitsValue;
    }
    void borderRightUnitsValue(uint8_t value)
    {
        if (m_BorderRightUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(borderRightUnitsValuePropertyKey,
                             &m_BorderRightUnitsValue,
                             &value);
        m_BorderRightUnitsValue = value;
        RIVE_EDITOR_CHANGED(borderRightUnitsValueChanged());
        notifyPropertyChanged(borderRightUnitsValuePropertyKey);
    }
#else
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
        m_border.ensureAllocated()->borderRightUnitsValue = value;
        borderRightUnitsValueChanged();
        notifyPropertyChanged(borderRightUnitsValuePropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline uint8_t borderTopUnitsValue() const { return m_BorderTopUnitsValue; }
    void borderTopUnitsValue(uint8_t value)
    {
        if (m_BorderTopUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(borderTopUnitsValuePropertyKey,
                             &m_BorderTopUnitsValue,
                             &value);
        m_BorderTopUnitsValue = value;
        RIVE_EDITOR_CHANGED(borderTopUnitsValueChanged());
        notifyPropertyChanged(borderTopUnitsValuePropertyKey);
    }
#else
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
        m_border.ensureAllocated()->borderTopUnitsValue = value;
        borderTopUnitsValueChanged();
        notifyPropertyChanged(borderTopUnitsValuePropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline uint8_t borderBottomUnitsValue() const
    {
        return m_BorderBottomUnitsValue;
    }
    void borderBottomUnitsValue(uint8_t value)
    {
        if (m_BorderBottomUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(borderBottomUnitsValuePropertyKey,
                             &m_BorderBottomUnitsValue,
                             &value);
        m_BorderBottomUnitsValue = value;
        RIVE_EDITOR_CHANGED(borderBottomUnitsValueChanged());
        notifyPropertyChanged(borderBottomUnitsValuePropertyKey);
    }
#else
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
        m_border.ensureAllocated()->borderBottomUnitsValue = value;
        borderBottomUnitsValueChanged();
        notifyPropertyChanged(borderBottomUnitsValuePropertyKey);
    }
#endif

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
        RIVE_EDITOR_CHANGING(marginLeftUnitsValuePropertyKey,
                             &m_MarginLeftUnitsValue,
                             &value);
        m_MarginLeftUnitsValue = value;
        RIVE_EDITOR_CHANGED(marginLeftUnitsValueChanged());
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
        RIVE_EDITOR_CHANGING(marginRightUnitsValuePropertyKey,
                             &m_MarginRightUnitsValue,
                             &value);
        m_MarginRightUnitsValue = value;
        RIVE_EDITOR_CHANGED(marginRightUnitsValueChanged());
        notifyPropertyChanged(marginRightUnitsValuePropertyKey);
    }

    inline uint8_t marginTopUnitsValue() const { return m_MarginTopUnitsValue; }
    void marginTopUnitsValue(uint8_t value)
    {
        if (m_MarginTopUnitsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(marginTopUnitsValuePropertyKey,
                             &m_MarginTopUnitsValue,
                             &value);
        m_MarginTopUnitsValue = value;
        RIVE_EDITOR_CHANGED(marginTopUnitsValueChanged());
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
        RIVE_EDITOR_CHANGING(marginBottomUnitsValuePropertyKey,
                             &m_MarginBottomUnitsValue,
                             &value);
        m_MarginBottomUnitsValue = value;
        RIVE_EDITOR_CHANGED(marginBottomUnitsValueChanged());
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
        RIVE_EDITOR_CHANGING(paddingLeftUnitsValuePropertyKey,
                             &m_PaddingLeftUnitsValue,
                             &value);
        m_PaddingLeftUnitsValue = value;
        RIVE_EDITOR_CHANGED(paddingLeftUnitsValueChanged());
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
        RIVE_EDITOR_CHANGING(paddingRightUnitsValuePropertyKey,
                             &m_PaddingRightUnitsValue,
                             &value);
        m_PaddingRightUnitsValue = value;
        RIVE_EDITOR_CHANGED(paddingRightUnitsValueChanged());
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
        RIVE_EDITOR_CHANGING(paddingTopUnitsValuePropertyKey,
                             &m_PaddingTopUnitsValue,
                             &value);
        m_PaddingTopUnitsValue = value;
        RIVE_EDITOR_CHANGED(paddingTopUnitsValueChanged());
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
        RIVE_EDITOR_CHANGING(paddingBottomUnitsValuePropertyKey,
                             &m_PaddingBottomUnitsValue,
                             &value);
        m_PaddingBottomUnitsValue = value;
        RIVE_EDITOR_CHANGED(paddingBottomUnitsValueChanged());
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
        RIVE_EDITOR_CHANGING(gapHorizontalUnitsValuePropertyKey,
                             &m_GapHorizontalUnitsValue,
                             &value);
        m_GapHorizontalUnitsValue = value;
        RIVE_EDITOR_CHANGED(gapHorizontalUnitsValueChanged());
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
        RIVE_EDITOR_CHANGING(gapVerticalUnitsValuePropertyKey,
                             &m_GapVerticalUnitsValue,
                             &value);
        m_GapVerticalUnitsValue = value;
        RIVE_EDITOR_CHANGED(gapVerticalUnitsValueChanged());
        notifyPropertyChanged(gapVerticalUnitsValuePropertyKey);
    }

    inline uint8_t justifyItemsValue() const { return m_JustifyItemsValue; }
    void justifyItemsValue(uint8_t value)
    {
        if (m_JustifyItemsValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(justifyItemsValuePropertyKey,
                             &m_JustifyItemsValue,
                             &value);
        m_JustifyItemsValue = value;
        RIVE_EDITOR_CHANGED(justifyItemsValueChanged());
        notifyPropertyChanged(justifyItemsValuePropertyKey);
    }

    inline uint8_t layoutTypeValue() const { return m_LayoutTypeValue; }
    void layoutTypeValue(uint8_t value)
    {
        if (m_LayoutTypeValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(layoutTypeValuePropertyKey,
                             &m_LayoutTypeValue,
                             &value);
        m_LayoutTypeValue = value;
        RIVE_EDITOR_CHANGED(layoutTypeValueChanged());
        notifyPropertyChanged(layoutTypeValuePropertyKey);
    }

#ifdef WITH_RIVE_EDITOR
    inline bool linkCornerRadius() const { return m_LinkCornerRadius; }
    void linkCornerRadius(bool value)
    {
        if (m_LinkCornerRadius == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(linkCornerRadiusPropertyKey,
                             &m_LinkCornerRadius,
                             &value);
        m_LinkCornerRadius = value;
        RIVE_EDITOR_CHANGED(linkCornerRadiusChanged());
        notifyPropertyChanged(linkCornerRadiusPropertyKey);
    }
#else
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
        m_cornerRadius.ensureAllocated()->linkCornerRadius = value;
        linkCornerRadiusChanged();
        notifyPropertyChanged(linkCornerRadiusPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline float cornerRadiusTL() const { return m_CornerRadiusTL; }
    void cornerRadiusTL(float value)
    {
        if (m_CornerRadiusTL == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(cornerRadiusTLPropertyKey,
                             &m_CornerRadiusTL,
                             &value);
        m_CornerRadiusTL = value;
        RIVE_EDITOR_CHANGED(cornerRadiusTLChanged());
        notifyPropertyChanged(cornerRadiusTLPropertyKey);
    }
#else
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
        m_cornerRadius.ensureAllocated()->cornerRadiusTL = value;
        cornerRadiusTLChanged();
        notifyPropertyChanged(cornerRadiusTLPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline float cornerRadiusTR() const { return m_CornerRadiusTR; }
    void cornerRadiusTR(float value)
    {
        if (m_CornerRadiusTR == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(cornerRadiusTRPropertyKey,
                             &m_CornerRadiusTR,
                             &value);
        m_CornerRadiusTR = value;
        RIVE_EDITOR_CHANGED(cornerRadiusTRChanged());
        notifyPropertyChanged(cornerRadiusTRPropertyKey);
    }
#else
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
        m_cornerRadius.ensureAllocated()->cornerRadiusTR = value;
        cornerRadiusTRChanged();
        notifyPropertyChanged(cornerRadiusTRPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline float cornerRadiusBL() const { return m_CornerRadiusBL; }
    void cornerRadiusBL(float value)
    {
        if (m_CornerRadiusBL == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(cornerRadiusBLPropertyKey,
                             &m_CornerRadiusBL,
                             &value);
        m_CornerRadiusBL = value;
        RIVE_EDITOR_CHANGED(cornerRadiusBLChanged());
        notifyPropertyChanged(cornerRadiusBLPropertyKey);
    }
#else
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
        m_cornerRadius.ensureAllocated()->cornerRadiusBL = value;
        cornerRadiusBLChanged();
        notifyPropertyChanged(cornerRadiusBLPropertyKey);
    }
#endif

#ifdef WITH_RIVE_EDITOR
    inline float cornerRadiusBR() const { return m_CornerRadiusBR; }
    void cornerRadiusBR(float value)
    {
        if (m_CornerRadiusBR == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(cornerRadiusBRPropertyKey,
                             &m_CornerRadiusBR,
                             &value);
        m_CornerRadiusBR = value;
        RIVE_EDITOR_CHANGED(cornerRadiusBRChanged());
        notifyPropertyChanged(cornerRadiusBRPropertyKey);
    }
#else
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
        m_cornerRadius.ensureAllocated()->cornerRadiusBR = value;
        cornerRadiusBRChanged();
        notifyPropertyChanged(cornerRadiusBRPropertyKey);
    }
#endif

    Core* clone() const override;
    void copy(const LayoutComponentStyleBase& object)
    {
        m_GapHorizontal = object.m_GapHorizontal;
        m_GapVertical = object.m_GapVertical;
#ifdef WITH_RIVE_EDITOR
        m_BorderLeft = object.m_BorderLeft;
#endif
#ifdef WITH_RIVE_EDITOR
        m_BorderRight = object.m_BorderRight;
#endif
#ifdef WITH_RIVE_EDITOR
        m_BorderTop = object.m_BorderTop;
#endif
#ifdef WITH_RIVE_EDITOR
        m_BorderBottom = object.m_BorderBottom;
#endif
        m_MarginLeft = object.m_MarginLeft;
        m_MarginRight = object.m_MarginRight;
        m_MarginTop = object.m_MarginTop;
        m_MarginBottom = object.m_MarginBottom;
        m_PaddingLeft = object.m_PaddingLeft;
        m_PaddingRight = object.m_PaddingRight;
        m_PaddingTop = object.m_PaddingTop;
        m_PaddingBottom = object.m_PaddingBottom;
#ifdef WITH_RIVE_EDITOR
        m_PositionLeft = object.m_PositionLeft;
#endif
#ifdef WITH_RIVE_EDITOR
        m_PositionRight = object.m_PositionRight;
#endif
#ifdef WITH_RIVE_EDITOR
        m_PositionTop = object.m_PositionTop;
#endif
#ifdef WITH_RIVE_EDITOR
        m_PositionBottom = object.m_PositionBottom;
#endif
#ifdef WITH_RIVE_EDITOR
        m_PositionLeftUnitsValue = object.m_PositionLeftUnitsValue;
#endif
#ifdef WITH_RIVE_EDITOR
        m_PositionRightUnitsValue = object.m_PositionRightUnitsValue;
#endif
#ifdef WITH_RIVE_EDITOR
        m_PositionTopUnitsValue = object.m_PositionTopUnitsValue;
#endif
#ifdef WITH_RIVE_EDITOR
        m_PositionBottomUnitsValue = object.m_PositionBottomUnitsValue;
#endif
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
#ifdef WITH_RIVE_EDITOR
        m_BorderLeftUnitsValue = object.m_BorderLeftUnitsValue;
#endif
#ifdef WITH_RIVE_EDITOR
        m_BorderRightUnitsValue = object.m_BorderRightUnitsValue;
#endif
#ifdef WITH_RIVE_EDITOR
        m_BorderTopUnitsValue = object.m_BorderTopUnitsValue;
#endif
#ifdef WITH_RIVE_EDITOR
        m_BorderBottomUnitsValue = object.m_BorderBottomUnitsValue;
#endif
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
#ifdef WITH_RIVE_EDITOR
        m_LinkCornerRadius = object.m_LinkCornerRadius;
#endif
#ifdef WITH_RIVE_EDITOR
        m_CornerRadiusTL = object.m_CornerRadiusTL;
#endif
#ifdef WITH_RIVE_EDITOR
        m_CornerRadiusTR = object.m_CornerRadiusTR;
#endif
#ifdef WITH_RIVE_EDITOR
        m_CornerRadiusBL = object.m_CornerRadiusBL;
#endif
#ifdef WITH_RIVE_EDITOR
        m_CornerRadiusBR = object.m_CornerRadiusBR;
#endif
#ifndef WITH_RIVE_EDITOR
        m_border = object.m_border;
        m_absolutePosition = object.m_absolutePosition;
        m_cornerRadius = object.m_cornerRadius;
#endif
        RIVE_EDITOR_COPY(object);
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
#ifdef WITH_RIVE_EDITOR
                m_BorderLeft = CoreDoubleType::deserialize(reader);
#else
                m_border.ensureAllocated()->borderLeft =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case borderRightPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_BorderRight = CoreDoubleType::deserialize(reader);
#else
                m_border.ensureAllocated()->borderRight =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case borderTopPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_BorderTop = CoreDoubleType::deserialize(reader);
#else
                m_border.ensureAllocated()->borderTop =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case borderBottomPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_BorderBottom = CoreDoubleType::deserialize(reader);
#else
                m_border.ensureAllocated()->borderBottom =
                    CoreDoubleType::deserialize(reader);
#endif
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
#ifdef WITH_RIVE_EDITOR
                m_PositionLeft = CoreDoubleType::deserialize(reader);
#else
                m_absolutePosition.ensureAllocated()->positionLeft =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case positionRightPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_PositionRight = CoreDoubleType::deserialize(reader);
#else
                m_absolutePosition.ensureAllocated()->positionRight =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case positionTopPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_PositionTop = CoreDoubleType::deserialize(reader);
#else
                m_absolutePosition.ensureAllocated()->positionTop =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case positionBottomPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_PositionBottom = CoreDoubleType::deserialize(reader);
#else
                m_absolutePosition.ensureAllocated()->positionBottom =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case positionLeftUnitsValuePropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_PositionLeftUnitsValue = CoreUintType::deserialize(reader);
#else
                m_absolutePosition.ensureAllocated()->positionLeftUnitsValue =
                    CoreUintType::deserialize(reader);
#endif
                return true;
            case positionRightUnitsValuePropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_PositionRightUnitsValue = CoreUintType::deserialize(reader);
#else
                m_absolutePosition.ensureAllocated()->positionRightUnitsValue =
                    CoreUintType::deserialize(reader);
#endif
                return true;
            case positionTopUnitsValuePropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_PositionTopUnitsValue = CoreUintType::deserialize(reader);
#else
                m_absolutePosition.ensureAllocated()->positionTopUnitsValue =
                    CoreUintType::deserialize(reader);
#endif
                return true;
            case positionBottomUnitsValuePropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_PositionBottomUnitsValue = CoreUintType::deserialize(reader);
#else
                m_absolutePosition.ensureAllocated()->positionBottomUnitsValue =
                    CoreUintType::deserialize(reader);
#endif
                return true;
            case flexBasisPropertyKey:
                m_FlexBasis = CoreDoubleType::deserialize(reader);
                return true;
            case aspectRatioPropertyKey:
                m_AspectRatio = CoreDoubleType::deserialize(reader);
                return true;
            case interpolatorIdPropertyKey:
                m_InterpolatorId = CoreIdType::runtimeDeserialize(reader);
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
#ifdef WITH_RIVE_EDITOR
                m_BorderLeftUnitsValue = CoreUintType::deserialize(reader);
#else
                m_border.ensureAllocated()->borderLeftUnitsValue =
                    CoreUintType::deserialize(reader);
#endif
                return true;
            case borderRightUnitsValuePropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_BorderRightUnitsValue = CoreUintType::deserialize(reader);
#else
                m_border.ensureAllocated()->borderRightUnitsValue =
                    CoreUintType::deserialize(reader);
#endif
                return true;
            case borderTopUnitsValuePropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_BorderTopUnitsValue = CoreUintType::deserialize(reader);
#else
                m_border.ensureAllocated()->borderTopUnitsValue =
                    CoreUintType::deserialize(reader);
#endif
                return true;
            case borderBottomUnitsValuePropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_BorderBottomUnitsValue = CoreUintType::deserialize(reader);
#else
                m_border.ensureAllocated()->borderBottomUnitsValue =
                    CoreUintType::deserialize(reader);
#endif
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
#ifdef WITH_RIVE_EDITOR
                m_LinkCornerRadius = CoreBoolType::deserialize(reader);
#else
                m_cornerRadius.ensureAllocated()->linkCornerRadius =
                    CoreBoolType::deserialize(reader);
#endif
                return true;
            case cornerRadiusTLPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_CornerRadiusTL = CoreDoubleType::deserialize(reader);
#else
                m_cornerRadius.ensureAllocated()->cornerRadiusTL =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case cornerRadiusTRPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_CornerRadiusTR = CoreDoubleType::deserialize(reader);
#else
                m_cornerRadius.ensureAllocated()->cornerRadiusTR =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case cornerRadiusBLPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_CornerRadiusBL = CoreDoubleType::deserialize(reader);
#else
                m_cornerRadius.ensureAllocated()->cornerRadiusBL =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
            case cornerRadiusBRPropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_CornerRadiusBR = CoreDoubleType::deserialize(reader);
#else
                m_cornerRadius.ensureAllocated()->cornerRadiusBR =
                    CoreDoubleType::deserialize(reader);
#endif
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
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
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/layout/layout_component_style_ext.inl"
#endif
};
} // namespace rive

#endif