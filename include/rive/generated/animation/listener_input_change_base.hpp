#ifndef _RIVE_LISTENER_INPUT_CHANGE_BASE_HPP_
#define _RIVE_LISTENER_INPUT_CHANGE_BASE_HPP_
#include "rive/animation/listener_action.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ListenerInputChangeBase : public ListenerAction
{
protected:
    typedef ListenerAction Super;

public:
    static const uint16_t typeKey = 116;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerInputChangeBase::typeKey:
            case ListenerActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t inputIdPropertyKey = 227;
    static const uint16_t nestedInputIdPropertyKey = 400;

private:
    uint32_t m_InputId = -1;
    uint32_t m_NestedInputId = -1;

public:
    inline uint32_t inputId() const { return m_InputId; }
    void inputId(uint32_t value)
    {
        if (m_InputId == value)
        {
            return;
        }
        m_InputId = value;
        inputIdChanged();
    }

    inline uint32_t nestedInputId() const { return m_NestedInputId; }
    void nestedInputId(uint32_t value)
    {
        if (m_NestedInputId == value)
        {
            return;
        }
        m_NestedInputId = value;
        nestedInputIdChanged();
    }

    void copy(const ListenerInputChangeBase& object)
    {
        m_InputId = object.m_InputId;
        m_NestedInputId = object.m_NestedInputId;
        ListenerAction::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case inputIdPropertyKey:
                m_InputId = CoreUintType::deserialize(reader);
                return true;
            case nestedInputIdPropertyKey:
                m_NestedInputId = CoreUintType::deserialize(reader);
                return true;
        }
        return ListenerAction::deserialize(propertyKey, reader);
    }

protected:
    virtual void inputIdChanged() {}
    virtual void nestedInputIdChanged() {}
};
} // namespace rive

#endif