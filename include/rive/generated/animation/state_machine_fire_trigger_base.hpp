#ifndef _RIVE_STATE_MACHINE_FIRE_TRIGGER_BASE_HPP_
#define _RIVE_STATE_MACHINE_FIRE_TRIGGER_BASE_HPP_
#include "rive/animation/state_machine_fire_action.hpp"
#include "rive/core/field_types/core_bytes_type.hpp"
#include "rive/span.hpp"
namespace rive
{
class StateMachineFireTriggerBase : public StateMachineFireAction
{
protected:
    typedef StateMachineFireAction Super;

public:
    static const uint16_t typeKey = 614;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StateMachineFireTriggerBase::typeKey:
            case StateMachineFireActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t viewModelPathIdsPropertyKey = 871;

public:
    virtual void decodeViewModelPathIds(Span<const uint8_t> value) = 0;
    virtual void copyViewModelPathIds(
        const StateMachineFireTriggerBase& object) = 0;

    Core* clone() const override;
    void copy(const StateMachineFireTriggerBase& object)
    {
        copyViewModelPathIds(object);
        StateMachineFireAction::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case viewModelPathIdsPropertyKey:
                decodeViewModelPathIds(CoreBytesType::deserialize(reader));
                return true;
        }
        return StateMachineFireAction::deserialize(propertyKey, reader);
    }

protected:
    virtual void viewModelPathIdsChanged() {}
};
} // namespace rive

#endif