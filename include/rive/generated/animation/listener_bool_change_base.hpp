#ifndef _RIVE_LISTENER_BOOL_CHANGE_BASE_HPP_
#define _RIVE_LISTENER_BOOL_CHANGE_BASE_HPP_
#include "rive/animation/listener_input_change.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ListenerBoolChangeBase : public ListenerInputChange
{
protected:
    typedef ListenerInputChange Super;

public:
    static const uint16_t typeKey = 117;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerBoolChangeBase::typeKey:
            case ListenerInputChangeBase::typeKey:
            case ListenerActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t valuePropertyKey = 228;

private:
    uint32_t m_Value = 1;

public:
    inline uint32_t value() const { return m_Value; }
    void value(uint32_t value)
    {
        if (m_Value == value)
        {
            return;
        }
        m_Value = value;
        valueChanged();
    }

    Core* clone() const override;
    void copy(const ListenerBoolChangeBase& object)
    {
        m_Value = object.m_Value;
        ListenerInputChange::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case valuePropertyKey:
                m_Value = CoreUintType::deserialize(reader);
                return true;
        }
        return ListenerInputChange::deserialize(propertyKey, reader);
    }

protected:
    virtual void valueChanged() {}
};
} // namespace rive

#endif