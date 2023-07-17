#ifndef _RIVE_TEXT_BASE_HPP_
#define _RIVE_TEXT_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/drawable.hpp"
namespace rive
{
class TextBase : public Drawable
{
protected:
    typedef Drawable Super;

public:
    static const uint16_t typeKey = 134;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextBase::typeKey:
            case DrawableBase::typeKey:
            case NodeBase::typeKey:
            case TransformComponentBase::typeKey:
            case WorldTransformComponentBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t alignValuePropertyKey = 281;
    static const uint16_t sizingValuePropertyKey = 284;
    static const uint16_t overflowValuePropertyKey = 287;
    static const uint16_t widthPropertyKey = 285;
    static const uint16_t heightPropertyKey = 286;
    static const uint16_t originXPropertyKey = 366;
    static const uint16_t originYPropertyKey = 367;
    static const uint16_t paragraphSpacingPropertyKey = 371;
    static const uint16_t originValuePropertyKey = 377;

private:
    uint32_t m_AlignValue = 0;
    uint32_t m_SizingValue = 0;
    uint32_t m_OverflowValue = 0;
    float m_Width = 0.0f;
    float m_Height = 0.0f;
    float m_OriginX = 0.0f;
    float m_OriginY = 0.0f;
    float m_ParagraphSpacing = 0.0f;
    uint32_t m_OriginValue = 0;

public:
    inline uint32_t alignValue() const { return m_AlignValue; }
    void alignValue(uint32_t value)
    {
        if (m_AlignValue == value)
        {
            return;
        }
        m_AlignValue = value;
        alignValueChanged();
    }

    inline uint32_t sizingValue() const { return m_SizingValue; }
    void sizingValue(uint32_t value)
    {
        if (m_SizingValue == value)
        {
            return;
        }
        m_SizingValue = value;
        sizingValueChanged();
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

    inline float width() const { return m_Width; }
    void width(float value)
    {
        if (m_Width == value)
        {
            return;
        }
        m_Width = value;
        widthChanged();
    }

    inline float height() const { return m_Height; }
    void height(float value)
    {
        if (m_Height == value)
        {
            return;
        }
        m_Height = value;
        heightChanged();
    }

    inline float originX() const { return m_OriginX; }
    void originX(float value)
    {
        if (m_OriginX == value)
        {
            return;
        }
        m_OriginX = value;
        originXChanged();
    }

    inline float originY() const { return m_OriginY; }
    void originY(float value)
    {
        if (m_OriginY == value)
        {
            return;
        }
        m_OriginY = value;
        originYChanged();
    }

    inline float paragraphSpacing() const { return m_ParagraphSpacing; }
    void paragraphSpacing(float value)
    {
        if (m_ParagraphSpacing == value)
        {
            return;
        }
        m_ParagraphSpacing = value;
        paragraphSpacingChanged();
    }

    inline uint32_t originValue() const { return m_OriginValue; }
    void originValue(uint32_t value)
    {
        if (m_OriginValue == value)
        {
            return;
        }
        m_OriginValue = value;
        originValueChanged();
    }

    Core* clone() const override;
    void copy(const TextBase& object)
    {
        m_AlignValue = object.m_AlignValue;
        m_SizingValue = object.m_SizingValue;
        m_OverflowValue = object.m_OverflowValue;
        m_Width = object.m_Width;
        m_Height = object.m_Height;
        m_OriginX = object.m_OriginX;
        m_OriginY = object.m_OriginY;
        m_ParagraphSpacing = object.m_ParagraphSpacing;
        m_OriginValue = object.m_OriginValue;
        Drawable::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case alignValuePropertyKey:
                m_AlignValue = CoreUintType::deserialize(reader);
                return true;
            case sizingValuePropertyKey:
                m_SizingValue = CoreUintType::deserialize(reader);
                return true;
            case overflowValuePropertyKey:
                m_OverflowValue = CoreUintType::deserialize(reader);
                return true;
            case widthPropertyKey:
                m_Width = CoreDoubleType::deserialize(reader);
                return true;
            case heightPropertyKey:
                m_Height = CoreDoubleType::deserialize(reader);
                return true;
            case originXPropertyKey:
                m_OriginX = CoreDoubleType::deserialize(reader);
                return true;
            case originYPropertyKey:
                m_OriginY = CoreDoubleType::deserialize(reader);
                return true;
            case paragraphSpacingPropertyKey:
                m_ParagraphSpacing = CoreDoubleType::deserialize(reader);
                return true;
            case originValuePropertyKey:
                m_OriginValue = CoreUintType::deserialize(reader);
                return true;
        }
        return Drawable::deserialize(propertyKey, reader);
    }

protected:
    virtual void alignValueChanged() {}
    virtual void sizingValueChanged() {}
    virtual void overflowValueChanged() {}
    virtual void widthChanged() {}
    virtual void heightChanged() {}
    virtual void originXChanged() {}
    virtual void originYChanged() {}
    virtual void paragraphSpacingChanged() {}
    virtual void originValueChanged() {}
};
} // namespace rive

#endif