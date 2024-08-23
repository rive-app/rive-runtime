#ifndef _RIVE_LISTENER_NUMBER_CHANGE_BASE_HPP_
#define _RIVE_LISTENER_NUMBER_CHANGE_BASE_HPP_
#include "rive/animation/listener_input_change.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class ListenerNumberChangeBase : public ListenerInputChange
{
protected:
    typedef ListenerInputChange Super;

public:
    static const uint16_t typeKey = 118;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerNumberChangeBase::typeKey:
            case ListenerInputChangeBase::typeKey:
            case ListenerActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t valuePropertyKey = 229;

private:
    float m_Value = 0.0f;

public:
    inline float value() const { return m_Value; }
    void value(float value)
    {
        if (m_Value == value)
        {
            return;
        }
        m_Value = value;
        valueChanged();
    }

    Core* clone() const override;
    void copy(const ListenerNumberChangeBase& object)
    {
        m_Value = object.m_Value;
        ListenerInputChange::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case valuePropertyKey:
                m_Value = CoreDoubleType::deserialize(reader);
                return true;
        }
        return ListenerInputChange::deserialize(propertyKey, reader);
    }

protected:
    virtual void valueChanged() {}
};
} // namespace rive

#endif