#ifndef _RIVE_LAYOUT_COMPONENT_STYLE_BASE_HPP_
#define _RIVE_LAYOUT_COMPONENT_STYLE_BASE_HPP_
#include "rive/component.hpp"
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

    static const uint16_t layoutFlags0PropertyKey = 495;
    static const uint16_t layoutFlags1PropertyKey = 496;
    static const uint16_t layoutFlags2PropertyKey = 497;
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

private:
    uint32_t m_LayoutFlags0 = 0x5000412;
    uint32_t m_LayoutFlags1 = 0x00;
    uint32_t m_LayoutFlags2 = 0x00;
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

public:
    inline uint32_t layoutFlags0() const { return m_LayoutFlags0; }
    void layoutFlags0(uint32_t value)
    {
        if (m_LayoutFlags0 == value)
        {
            return;
        }
        m_LayoutFlags0 = value;
        layoutFlags0Changed();
    }

    inline uint32_t layoutFlags1() const { return m_LayoutFlags1; }
    void layoutFlags1(uint32_t value)
    {
        if (m_LayoutFlags1 == value)
        {
            return;
        }
        m_LayoutFlags1 = value;
        layoutFlags1Changed();
    }

    inline uint32_t layoutFlags2() const { return m_LayoutFlags2; }
    void layoutFlags2(uint32_t value)
    {
        if (m_LayoutFlags2 == value)
        {
            return;
        }
        m_LayoutFlags2 = value;
        layoutFlags2Changed();
    }

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

    Core* clone() const override;
    void copy(const LayoutComponentStyleBase& object)
    {
        m_LayoutFlags0 = object.m_LayoutFlags0;
        m_LayoutFlags1 = object.m_LayoutFlags1;
        m_LayoutFlags2 = object.m_LayoutFlags2;
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
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case layoutFlags0PropertyKey:
                m_LayoutFlags0 = CoreUintType::deserialize(reader);
                return true;
            case layoutFlags1PropertyKey:
                m_LayoutFlags1 = CoreUintType::deserialize(reader);
                return true;
            case layoutFlags2PropertyKey:
                m_LayoutFlags2 = CoreUintType::deserialize(reader);
                return true;
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
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void layoutFlags0Changed() {}
    virtual void layoutFlags1Changed() {}
    virtual void layoutFlags2Changed() {}
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
};
} // namespace rive

#endif