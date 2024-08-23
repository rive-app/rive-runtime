#ifndef _RIVE_ANIMATION_STATE_BASE_HPP_
#define _RIVE_ANIMATION_STATE_BASE_HPP_
#include "rive/animation/advanceable_state.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class AnimationStateBase : public AdvanceableState
{
protected:
    typedef AdvanceableState Super;

public:
    static const uint16_t typeKey = 61;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
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

private:
    uint32_t m_AnimationId = -1;

public:
    inline uint32_t animationId() const { return m_AnimationId; }
    void animationId(uint32_t value)
    {
        if (m_AnimationId == value)
        {
            return;
        }
        m_AnimationId = value;
        animationIdChanged();
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
                m_AnimationId = CoreUintType::deserialize(reader);
                return true;
        }
        return AdvanceableState::deserialize(propertyKey, reader);
    }

protected:
    virtual void animationIdChanged() {}
};
} // namespace rive

#endif