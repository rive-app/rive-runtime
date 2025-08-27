#ifndef _RIVE_DASH_PATH_BASE_HPP_
#define _RIVE_DASH_PATH_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class DashPathBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 506;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DashPathBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t offsetPropertyKey = 690;
    static const uint16_t offsetIsPercentagePropertyKey = 691;

protected:
    float m_Offset = 0.0f;
    bool m_OffsetIsPercentage = false;

public:
    inline float offset() const { return m_Offset; }
    void offset(float value)
    {
        if (m_Offset == value)
        {
            return;
        }
        m_Offset = value;
        offsetChanged();
    }

    inline bool offsetIsPercentage() const { return m_OffsetIsPercentage; }
    void offsetIsPercentage(bool value)
    {
        if (m_OffsetIsPercentage == value)
        {
            return;
        }
        m_OffsetIsPercentage = value;
        offsetIsPercentageChanged();
    }

    Core* clone() const override;
    void copy(const DashPathBase& object)
    {
        m_Offset = object.m_Offset;
        m_OffsetIsPercentage = object.m_OffsetIsPercentage;
        ContainerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case offsetPropertyKey:
                m_Offset = CoreDoubleType::deserialize(reader);
                return true;
            case offsetIsPercentagePropertyKey:
                m_OffsetIsPercentage = CoreBoolType::deserialize(reader);
                return true;
        }
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void offsetChanged() {}
    virtual void offsetIsPercentageChanged() {}
};
} // namespace rive

#endif