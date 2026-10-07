#ifndef _RIVE_STATE_MACHINE_LISTENER_BASE_HPP_
#define _RIVE_STATE_MACHINE_LISTENER_BASE_HPP_
#include "rive/animation/state_machine_component.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class StateMachineListenerBase : public StateMachineComponent
{
protected:
    typedef StateMachineComponent Super;

public:
    static const uint16_t typeKey = 654;

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t targetIdPropertyKey = 224;

protected:
    Id m_TargetId = kEmptyId;

public:
    inline Id targetId() const { return m_TargetId; }
    void targetId(Id value)
    {
        if (m_TargetId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(targetIdPropertyKey, &m_TargetId, &value);
        m_TargetId = value;
        notifyPropertyChanged(targetIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const StateMachineListenerBase& object)
    {
        m_TargetId = object.m_TargetId;
        StateMachineComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case targetIdPropertyKey:
                m_TargetId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return StateMachineComponent::deserialize(propertyKey, reader);
    }

protected:
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/state_machine_listener_ext.inl"
#endif
};
} // namespace rive

#endif