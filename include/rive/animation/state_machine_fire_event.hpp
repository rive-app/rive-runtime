#ifndef _RIVE_STATE_MACHINE_FIRE_EVENT_HPP_
#define _RIVE_STATE_MACHINE_FIRE_EVENT_HPP_
#include "rive/generated/animation/state_machine_fire_event_base.hpp"

namespace rive
{

class StateMachineFireEvent : public StateMachineFireEventBase
{
public:
    void perform(StateMachineInstance* stateMachineInstance) const override;
};

} // namespace rive
#endif