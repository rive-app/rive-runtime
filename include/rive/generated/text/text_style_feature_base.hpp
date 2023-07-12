#ifndef _RIVE_TEXT_STYLE_FEATURE_BASE_HPP_
#define _RIVE_TEXT_STYLE_FEATURE_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TextStyleFeatureBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 164;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextStyleFeatureBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t tagPropertyKey = 356;
    static const uint16_t featureValuePropertyKey = 357;

private:
    uint32_t m_Tag = 0;
    uint32_t m_FeatureValue = 1;

public:
    inline uint32_t tag() const { return m_Tag; }
    void tag(uint32_t value)
    {
        if (m_Tag == value)
        {
            return;
        }
        m_Tag = value;
        tagChanged();
    }

    inline uint32_t featureValue() const { return m_FeatureValue; }
    void featureValue(uint32_t value)
    {
        if (m_FeatureValue == value)
        {
            return;
        }
        m_FeatureValue = value;
        featureValueChanged();
    }

    Core* clone() const override;
    void copy(const TextStyleFeatureBase& object)
    {
        m_Tag = object.m_Tag;
        m_FeatureValue = object.m_FeatureValue;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case tagPropertyKey:
                m_Tag = CoreUintType::deserialize(reader);
                return true;
            case featureValuePropertyKey:
                m_FeatureValue = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void tagChanged() {}
    virtual void featureValueChanged() {}
};
} // namespace rive

#endif