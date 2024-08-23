#ifndef _RIVE_BLEND_STATE_TRANSITION_BASE_HPP_
#define _RIVE_BLEND_STATE_TRANSITION_BASE_HPP_
#include "rive/animation/state_transition.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class BlendStateTransitionBase : public StateTransition
{
protected:
    typedef StateTransition Super;

public:
    static const uint16_t typeKey = 78;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
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

private:
    uint32_t m_ExitBlendAnimationId = -1;

public:
    inline uint32_t exitBlendAnimationId() const { return m_ExitBlendAnimationId; }
    void exitBlendAnimationId(uint32_t value)
    {
        if (m_ExitBlendAnimationId == value)
        {
            return;
        }
        m_ExitBlendAnimationId = value;
        exitBlendAnimationIdChanged();
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
                m_ExitBlendAnimationId = CoreUintType::deserialize(reader);
                return true;
        }
        return StateTransition::deserialize(propertyKey, reader);
    }

protected:
    virtual void exitBlendAnimationIdChanged() {}
};
} // namespace rive

#endif