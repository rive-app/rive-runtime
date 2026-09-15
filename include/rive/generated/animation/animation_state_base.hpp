#ifndef _RIVE_ANIMATION_STATE_BASE_HPP_
#define _RIVE_ANIMATION_STATE_BASE_HPP_
#include "rive/animation/advanceable_state.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class AnimationStateBase : public AdvanceableState
{
protected:
    typedef AdvanceableState Super;

public:
    static const uint16_t typeKey = 61;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case AnimationStateBase::typeKey:
            case AdvanceableStateBase::typeKey:
            case LayerStateBase::typeKey:
            case StateMachineLayerComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t animationIdPropertyKey = 149;

protected:
    Id m_AnimationId = kEmptyId;

public:
    inline Id animationId() const { return m_AnimationId; }
    void animationId(Id value)
    {
        if (m_AnimationId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(animationIdPropertyKey, &m_AnimationId, &value);
        m_AnimationId = value;
        RIVE_EDITOR_CHANGED(animationIdChanged());
        notifyPropertyChanged(animationIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const AnimationStateBase& object)
    {
        m_AnimationId = object.m_AnimationId;
        AdvanceableState::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case animationIdPropertyKey:
                m_AnimationId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return AdvanceableState::deserialize(propertyKey, reader);
    }

protected:
    virtual void animationIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/animation_state_ext.inl"
#endif
};
} // namespace rive

#endif