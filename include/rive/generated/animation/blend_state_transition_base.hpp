#ifndef _RIVE_BLEND_STATE_TRANSITION_BASE_HPP_
#define _RIVE_BLEND_STATE_TRANSITION_BASE_HPP_
#include "rive/animation/state_transition.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class BlendStateTransitionBase : public StateTransition
{
protected:
    typedef StateTransition Super;

public:
    static const uint16_t typeKey = 78;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case BlendStateTransitionBase::typeKey:
            case StateTransitionBase::typeKey:
            case StateMachineLayerComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t exitBlendAnimationIdPropertyKey = 171;

protected:
    Id m_ExitBlendAnimationId = kEmptyId;

public:
    inline Id exitBlendAnimationId() const { return m_ExitBlendAnimationId; }
    void exitBlendAnimationId(Id value)
    {
        if (m_ExitBlendAnimationId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(exitBlendAnimationIdPropertyKey,
                             &m_ExitBlendAnimationId,
                             &value);
        m_ExitBlendAnimationId = value;
        RIVE_EDITOR_CHANGED(exitBlendAnimationIdChanged());
        notifyPropertyChanged(exitBlendAnimationIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const BlendStateTransitionBase& object)
    {
        m_ExitBlendAnimationId = object.m_ExitBlendAnimationId;
        StateTransition::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case exitBlendAnimationIdPropertyKey:
                m_ExitBlendAnimationId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return StateTransition::deserialize(propertyKey, reader);
    }

protected:
    virtual void exitBlendAnimationIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/blend_state_transition_ext.inl"
#endif
};
} // namespace rive

#endif