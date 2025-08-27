#ifndef _RIVE_STATE_MACHINE_FIRE_TRIGGER_HPP_
#define _RIVE_STATE_MACHINE_FIRE_TRIGGER_HPP_
#include "rive/generated/animation/state_machine_fire_trigger_base.hpp"
#include <stdio.h>
namespace rive
{
class StateMachineFireTrigger : public StateMachineFireTriggerBase
{
public:
    void perform(StateMachineInstance* stateMachineInstance) const override;
    void decodeViewModelPathIds(Span<const uint8_t> value) override;
    void copyViewModelPathIds(
        const StateMachineFireTriggerBase& object) override;

protected:
    std::vector<uint32_t> m_viewModelPathIdsBuffer;
};
} // namespace rive

#endif