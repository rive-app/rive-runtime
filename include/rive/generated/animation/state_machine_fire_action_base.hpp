#ifndef _RIVE_STATE_MACHINE_FIRE_ACTION_BASE_HPP_
#define _RIVE_STATE_MACHINE_FIRE_ACTION_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class StateMachineFireActionBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 615;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StateMachineFireActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t occursValuePropertyKey = 393;

protected:
    uint32_t m_OccursValue = 0;

public:
    inline uint32_t occursValue() const { return m_OccursValue; }
    void occursValue(uint32_t value)
    {
        if (m_OccursValue == value)
        {
            return;
        }
        m_OccursValue = value;
        occursValueChanged();
    }

    void copy(const StateMachineFireActionBase& object)
    {
        m_OccursValue = object.m_OccursValue;
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case occursValuePropertyKey:
                m_OccursValue = CoreUintType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void occursValueChanged() {}
};
} // namespace rive

#endif