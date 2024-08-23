#ifndef _RIVE_STATE_MACHINE_FIRE_EVENT_HPP_
#define _RIVE_STATE_MACHINE_FIRE_EVENT_HPP_
#include "rive/generated/animation/state_machine_fire_event_base.hpp"

namespace rive
{
class StateMachineInstance;
enum class StateMachineFireOccurance : int
{
    atStart = 0,
    atEnd = 1
};

class StateMachineFireEvent : public StateMachineFireEventBase
{
public:
    StatusCode import(ImportStack& importStack) override;
    StateMachineFireOccurance occurs() const { return (StateMachineFireOccurance)occursValue(); }
    void perform(StateMachineInstance* stateMachineInstance) const;
};

} // namespace rive
#endif