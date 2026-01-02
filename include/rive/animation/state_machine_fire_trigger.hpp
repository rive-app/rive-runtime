#ifndef _RIVE_STATE_MACHINE_FIRE_TRIGGER_HPP_
#define _RIVE_STATE_MACHINE_FIRE_TRIGGER_HPP_
#include "rive/generated/animation/state_machine_fire_trigger_base.hpp"
#include "rive/data_bind_path_referencer.hpp"
#include <stdio.h>
namespace rive
{
class StateMachineFireTrigger : public StateMachineFireTriggerBase,
                                public DataBindPathReferencer
{
public:
    void perform(StateMachineInstance* stateMachineInstance) const override;
    void decodeViewModelPathIds(Span<const uint8_t> value) override;
    void copyViewModelPathIds(
        const StateMachineFireTriggerBase& object) override;
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif