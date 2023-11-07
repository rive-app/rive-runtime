#ifndef _RIVE_LISTENER_TRIGGER_CHANGE_HPP_
#define _RIVE_LISTENER_TRIGGER_CHANGE_HPP_
#include "rive/generated/animation/listener_trigger_change_base.hpp"

namespace rive
{
class NestedInput;
class ListenerTriggerChange : public ListenerTriggerChangeBase
{
public:
    bool validateInputType(const StateMachineInput* input) const override;
    bool validateNestedInputType(const NestedInput* input) const override;
    void perform(StateMachineInstance* stateMachineInstance, Vec2D position) const override;
};
} // namespace rive

#endif