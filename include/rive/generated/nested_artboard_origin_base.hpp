#ifndef _RIVE_NESTED_ARTBOARD_ORIGIN_BASE_HPP_
#define _RIVE_NESTED_ARTBOARD_ORIGIN_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class NestedArtboardOriginBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 1039;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NestedArtboardOriginBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t originXPropertyKey = 1040;
    static const uint16_t originYPropertyKey = 1041;

protected:
    float m_OriginX = 0.0f;
    float m_OriginY = 0.0f;

public:
    inline float originX() const { return m_OriginX; }
    void originX(float value)
    {
        if (m_OriginX == value)
        {
            return;
        }
        m_OriginX = value;
        originXChanged();
        notifyPropertyChanged(originXPropertyKey);
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
        notifyPropertyChanged(originYPropertyKey);
    }

    Core* clone() const override;
    void copy(const NestedArtboardOriginBase& object)
    {
        m_OriginX = object.m_OriginX;
        m_OriginY = object.m_OriginY;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case originXPropertyKey:
                m_OriginX = CoreDoubleType::deserialize(reader);
                return true;
            case originYPropertyKey:
                m_OriginY = CoreDoubleType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void originXChanged() {}
    virtual void originYChanged() {}
};
} // namespace rive

#endif