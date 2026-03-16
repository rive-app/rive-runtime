#ifndef _RIVE_LISTENER_INPUT_TYPE_BASE_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ListenerInputTypeBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 658;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerInputTypeBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t listenerTypeValuePropertyKey = 965;

protected:
    uint32_t m_ListenerTypeValue = 0;

public:
    inline uint32_t listenerTypeValue() const { return m_ListenerTypeValue; }
    void listenerTypeValue(uint32_t value)
    {
        if (m_ListenerTypeValue == value)
        {
            return;
        }
        m_ListenerTypeValue = value;
        listenerTypeValueChanged();
    }

    Core* clone() const override;
    void copy(const ListenerInputTypeBase& object)
    {
        m_ListenerTypeValue = object.m_ListenerTypeValue;
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case listenerTypeValuePropertyKey:
                m_ListenerTypeValue = CoreUintType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void listenerTypeValueChanged() {}
};
} // namespace rive

#endif