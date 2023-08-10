#ifndef _RIVE_TEXT_STYLE_BASE_HPP_
#define _RIVE_TEXT_STYLE_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TextStyleBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 137;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextStyleBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t fontSizePropertyKey = 274;
    static const uint16_t lineHeightPropertyKey = 370;
    static const uint16_t letterSpacingPropertyKey = 390;
    static const uint16_t fontAssetIdPropertyKey = 279;

private:
    float m_FontSize = 12.0f;
    float m_LineHeight = -1.0f;
    float m_LetterSpacing = 0.0f;
    uint32_t m_FontAssetId = -1;

public:
    inline float fontSize() const { return m_FontSize; }
    void fontSize(float value)
    {
        if (m_FontSize == value)
        {
            return;
        }
        m_FontSize = value;
        fontSizeChanged();
    }

    inline float lineHeight() const { return m_LineHeight; }
    void lineHeight(float value)
    {
        if (m_LineHeight == value)
        {
            return;
        }
        m_LineHeight = value;
        lineHeightChanged();
    }

    inline float letterSpacing() const { return m_LetterSpacing; }
    void letterSpacing(float value)
    {
        if (m_LetterSpacing == value)
        {
            return;
        }
        m_LetterSpacing = value;
        letterSpacingChanged();
    }

    inline uint32_t fontAssetId() const { return m_FontAssetId; }
    void fontAssetId(uint32_t value)
    {
        if (m_FontAssetId == value)
        {
            return;
        }
        m_FontAssetId = value;
        fontAssetIdChanged();
    }

    Core* clone() const override;
    void copy(const TextStyleBase& object)
    {
        m_FontSize = object.m_FontSize;
        m_LineHeight = object.m_LineHeight;
        m_LetterSpacing = object.m_LetterSpacing;
        m_FontAssetId = object.m_FontAssetId;
        ContainerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case fontSizePropertyKey:
                m_FontSize = CoreDoubleType::deserialize(reader);
                return true;
            case lineHeightPropertyKey:
                m_LineHeight = CoreDoubleType::deserialize(reader);
                return true;
            case letterSpacingPropertyKey:
                m_LetterSpacing = CoreDoubleType::deserialize(reader);
                return true;
            case fontAssetIdPropertyKey:
                m_FontAssetId = CoreUintType::deserialize(reader);
                return true;
        }
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void fontSizeChanged() {}
    virtual void lineHeightChanged() {}
    virtual void letterSpacingChanged() {}
    virtual void fontAssetIdChanged() {}
};
} // namespace rive

#endif