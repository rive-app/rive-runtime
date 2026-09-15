#ifndef _RIVE_STATE_MACHINE_BASE_HPP_
#define _RIVE_STATE_MACHINE_BASE_HPP_
#include "rive/animation/animation.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class StateMachineBase : public Animation
{
protected:
    typedef Animation Super;

public:
    static const uint16_t typeKey = 53;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StateMachineBase::typeKey:
            case AnimationBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

public:
    Core* clone() const override;
    void copy(const StateMachineBase& object)
    {
        RIVE_EDITOR_COPY(object);
        Animation::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return Animation::deserialize(propertyKey, reader);
    }
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/state_machine_ext.inl"
#endif
};
} // namespace rive

#endif