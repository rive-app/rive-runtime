#ifndef _RIVE_LISTENER_ACTION_HPP_
#define _RIVE_LISTENER_ACTION_HPP_
#include "rive/generated/animation/listener_action_base.hpp"
#include "rive/math/vec2d.hpp"

namespace rive
{
class StateMachineInstance;
class ListenerAction : public ListenerActionBase
{
public:
    StatusCode import(ImportStack& importStack) override;
    virtual void perform(StateMachineInstance* stateMachineInstance,
                         Vec2D position,
                         Vec2D previousPosition) const = 0;
};
} // namespace rive

#endif