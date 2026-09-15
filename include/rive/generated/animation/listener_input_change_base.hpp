#ifndef _RIVE_LISTENER_INPUT_CHANGE_BASE_HPP_
#define _RIVE_LISTENER_INPUT_CHANGE_BASE_HPP_
#include "rive/animation/listener_action.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class ListenerInputChangeBase : public ListenerAction
{
protected:
    typedef ListenerAction Super;

public:
    static const uint16_t typeKey = 116;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
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

protected:
    Id m_InputId = kEmptyId;
    Id m_NestedInputId = kEmptyId;

public:
    inline Id inputId() const { return m_InputId; }
    void inputId(Id value)
    {
        if (m_InputId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(inputIdPropertyKey, &m_InputId, &value);
        m_InputId = value;
        RIVE_EDITOR_CHANGED(inputIdChanged());
        notifyPropertyChanged(inputIdPropertyKey);
    }

    inline Id nestedInputId() const { return m_NestedInputId; }
    void nestedInputId(Id value)
    {
        if (m_NestedInputId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(nestedInputIdPropertyKey,
                             &m_NestedInputId,
                             &value);
        m_NestedInputId = value;
        RIVE_EDITOR_CHANGED(nestedInputIdChanged());
        notifyPropertyChanged(nestedInputIdPropertyKey);
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
                m_InputId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case nestedInputIdPropertyKey:
                m_NestedInputId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return ListenerAction::deserialize(propertyKey, reader);
    }

protected:
    virtual void inputIdChanged() {}
    virtual void nestedInputIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/listener_input_change_ext.inl"
#endif
};
} // namespace rive

#endif