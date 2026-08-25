#ifndef _RIVE_STATE_MACHINE_INPUT_BASE_HPP_
#define _RIVE_STATE_MACHINE_INPUT_BASE_HPP_
#include "rive/animation/state_machine_component.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class StateMachineInputBase : public StateMachineComponent
{
protected:
    typedef StateMachineComponent Super;

public:
    static const uint16_t typeKey = 55;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StateMachineInputBase::typeKey:
            case StateMachineComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

public:
    void copy(const StateMachineInputBase& object)
    {
        RIVE_EDITOR_COPY(object);
        StateMachineComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return StateMachineComponent::deserialize(propertyKey, reader);
    }
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/state_machine_input_ext.inl"
#endif
};
} // namespace rive

#endif