#ifndef _RIVE_TEXT_BASE_HPP_
#define _RIVE_TEXT_BASE_HPP_
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/id.hpp"
#include "rive/drawable.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class TextBase : public Drawable
{
protected:
    typedef Drawable Super;

public:
    static const uint16_t typeKey = 134;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
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
    static const uint16_t wrapValuePropertyKey = 683;
    static const uint16_t verticalAlignValuePropertyKey = 685;
    static const uint16_t fitFromBaselinePropertyKey = 703;
    static const uint16_t textRunListSourcePropertyKey = 932;
    static const uint16_t verticalTrimValuePropertyKey = 1026;
    static const uint16_t verticalTrimTopValuePropertyKey = 1027;
    static const uint32_t verticalTrimTopValueBitOffset = 0;
    static const uint32_t verticalTrimTopValueFieldMask = 255u;
    static const uint16_t verticalTrimBottomValuePropertyKey = 1028;
    static const uint32_t verticalTrimBottomValueBitOffset = 8;
    static const uint32_t verticalTrimBottomValueFieldMask = 65280u;

protected:
    uint32_t m_AlignValue = 0;
    uint32_t m_SizingValue = 0;
    uint32_t m_OverflowValue = 0;
    float m_Width = 0.0f;
    float m_Height = 0.0f;
    float m_OriginX = 0.0f;
    float m_OriginY = 0.0f;
    float m_ParagraphSpacing = 0.0f;
    uint32_t m_OriginValue = 0;
    uint32_t m_WrapValue = 0;
    uint32_t m_VerticalAlignValue = 0;
    bool m_FitFromBaseline = true;
    Id m_TextRunListSource = kEmptyId;
    uint32_t m_VerticalTrimValue = 0;

public:
    inline uint32_t alignValue() const { return m_AlignValue; }
    void alignValue(uint32_t value)
    {
        if (m_AlignValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(alignValuePropertyKey, &m_AlignValue, &value);
        m_AlignValue = value;
        RIVE_EDITOR_CHANGED(alignValueChanged());
        notifyPropertyChanged(alignValuePropertyKey);
    }

    inline uint32_t sizingValue() const { return m_SizingValue; }
    void sizingValue(uint32_t value)
    {
        if (m_SizingValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(sizingValuePropertyKey, &m_SizingValue, &value);
        m_SizingValue = value;
        RIVE_EDITOR_CHANGED(sizingValueChanged());
        notifyPropertyChanged(sizingValuePropertyKey);
    }

    inline uint32_t overflowValue() const { return m_OverflowValue; }
    void overflowValue(uint32_t value)
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

    inline float width() const { return m_Width; }
    void width(float value)
    {
        if (m_Width == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(widthPropertyKey, &m_Width, &value);
        m_Width = value;
        RIVE_EDITOR_CHANGED(widthChanged());
        notifyPropertyChanged(widthPropertyKey);
    }

    inline float height() const { return m_Height; }
    void height(float value)
    {
        if (m_Height == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(heightPropertyKey, &m_Height, &value);
        m_Height = value;
        RIVE_EDITOR_CHANGED(heightChanged());
        notifyPropertyChanged(heightPropertyKey);
    }

    inline float originX() const { return m_OriginX; }
    void originX(float value)
    {
        if (m_OriginX == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(originXPropertyKey, &m_OriginX, &value);
        m_OriginX = value;
        RIVE_EDITOR_CHANGED(originXChanged());
        notifyPropertyChanged(originXPropertyKey);
    }

    inline float originY() const { return m_OriginY; }
    void originY(float value)
    {
        if (m_OriginY == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(originYPropertyKey, &m_OriginY, &value);
        m_OriginY = value;
        RIVE_EDITOR_CHANGED(originYChanged());
        notifyPropertyChanged(originYPropertyKey);
    }

    inline float paragraphSpacing() const { return m_ParagraphSpacing; }
    void paragraphSpacing(float value)
    {
        if (m_ParagraphSpacing == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(paragraphSpacingPropertyKey,
                             &m_ParagraphSpacing,
                             &value);
        m_ParagraphSpacing = value;
        RIVE_EDITOR_CHANGED(paragraphSpacingChanged());
        notifyPropertyChanged(paragraphSpacingPropertyKey);
    }

    inline uint32_t originValue() const { return m_OriginValue; }
    void originValue(uint32_t value)
    {
        if (m_OriginValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(originValuePropertyKey, &m_OriginValue, &value);
        m_OriginValue = value;
        RIVE_EDITOR_CHANGED(originValueChanged());
        notifyPropertyChanged(originValuePropertyKey);
    }

    inline uint32_t wrapValue() const { return m_WrapValue; }
    void wrapValue(uint32_t value)
    {
        if (m_WrapValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(wrapValuePropertyKey, &m_WrapValue, &value);
        m_WrapValue = value;
        RIVE_EDITOR_CHANGED(wrapValueChanged());
        notifyPropertyChanged(wrapValuePropertyKey);
    }

    inline uint32_t verticalAlignValue() const { return m_VerticalAlignValue; }
    void verticalAlignValue(uint32_t value)
    {
        if (m_VerticalAlignValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(verticalAlignValuePropertyKey,
                             &m_VerticalAlignValue,
                             &value);
        m_VerticalAlignValue = value;
        RIVE_EDITOR_CHANGED(verticalAlignValueChanged());
        notifyPropertyChanged(verticalAlignValuePropertyKey);
    }

    inline bool fitFromBaseline() const { return m_FitFromBaseline; }
    void fitFromBaseline(bool value)
    {
        if (m_FitFromBaseline == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(fitFromBaselinePropertyKey,
                             &m_FitFromBaseline,
                             &value);
        m_FitFromBaseline = value;
        RIVE_EDITOR_CHANGED(fitFromBaselineChanged());
        notifyPropertyChanged(fitFromBaselinePropertyKey);
    }

    inline Id textRunListSource() const { return m_TextRunListSource; }
    void textRunListSource(Id value)
    {
        if (m_TextRunListSource == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(textRunListSourcePropertyKey,
                             &m_TextRunListSource,
                             &value);
        m_TextRunListSource = value;
        RIVE_EDITOR_CHANGED(textRunListSourceChanged());
        notifyPropertyChanged(textRunListSourcePropertyKey);
    }

    inline uint32_t verticalTrimValue() const { return m_VerticalTrimValue; }
    void verticalTrimValue(uint32_t value)
    {
        if (m_VerticalTrimValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(verticalTrimValuePropertyKey,
                             &m_VerticalTrimValue,
                             &value);
        m_VerticalTrimValue = value;
        RIVE_EDITOR_CHANGED(verticalTrimValueChanged());
        notifyPropertyChanged(verticalTrimValuePropertyKey);
    }

    inline uint32_t verticalTrimTopValue() const
    {
        return (m_VerticalTrimValue & verticalTrimTopValueFieldMask) >>
               verticalTrimTopValueBitOffset;
    }
    void verticalTrimTopValue(uint32_t value)
    {
        const uint32_t prev =
            (m_VerticalTrimValue & verticalTrimTopValueFieldMask) >>
            verticalTrimTopValueBitOffset;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(verticalTrimTopValuePropertyKey, &prev, &value);
        m_VerticalTrimValue =
            (m_VerticalTrimValue & ~verticalTrimTopValueFieldMask) |
            ((value << verticalTrimTopValueBitOffset) &
             verticalTrimTopValueFieldMask);
        RIVE_EDITOR_CHANGED(verticalTrimValueChanged());
        notifyPropertyChanged(verticalTrimValuePropertyKey);
    }
    inline uint32_t verticalTrimBottomValue() const
    {
        return (m_VerticalTrimValue & verticalTrimBottomValueFieldMask) >>
               verticalTrimBottomValueBitOffset;
    }
    void verticalTrimBottomValue(uint32_t value)
    {
        const uint32_t prev =
            (m_VerticalTrimValue & verticalTrimBottomValueFieldMask) >>
            verticalTrimBottomValueBitOffset;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(verticalTrimBottomValuePropertyKey, &prev, &value);
        m_VerticalTrimValue =
            (m_VerticalTrimValue & ~verticalTrimBottomValueFieldMask) |
            ((value << verticalTrimBottomValueBitOffset) &
             verticalTrimBottomValueFieldMask);
        RIVE_EDITOR_CHANGED(verticalTrimValueChanged());
        notifyPropertyChanged(verticalTrimValuePropertyKey);
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
        m_WrapValue = object.m_WrapValue;
        m_VerticalAlignValue = object.m_VerticalAlignValue;
        m_FitFromBaseline = object.m_FitFromBaseline;
        m_TextRunListSource = object.m_TextRunListSource;
        m_VerticalTrimValue = object.m_VerticalTrimValue;
        RIVE_EDITOR_COPY(object);
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
            case wrapValuePropertyKey:
                m_WrapValue = CoreUintType::deserialize(reader);
                return true;
            case verticalAlignValuePropertyKey:
                m_VerticalAlignValue = CoreUintType::deserialize(reader);
                return true;
            case fitFromBaselinePropertyKey:
                m_FitFromBaseline = CoreBoolType::deserialize(reader);
                return true;
            case textRunListSourcePropertyKey:
                m_TextRunListSource = CoreIdType::runtimeDeserialize(reader);
                return true;
            case verticalTrimValuePropertyKey:
                m_VerticalTrimValue = CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
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
    virtual void wrapValueChanged() {}
    virtual void verticalAlignValueChanged() {}
    virtual void fitFromBaselineChanged() {}
    virtual void textRunListSourceChanged() {}
    virtual void verticalTrimValueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/text/text_ext.inl"
#endif
};
} // namespace rive

#endif