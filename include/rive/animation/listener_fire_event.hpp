#ifndef _RIVE_LISTENER_FIRE_EVENT_HPP_
#define _RIVE_LISTENER_FIRE_EVENT_HPP_
#include "rive/generated/animation/listener_fire_event_base.hpp"

namespace rive
{
class ListenerFireEvent : public ListenerFireEventBase
{
public:
    void perform(StateMachineInstance* stateMachineInstance, Vec2D position) const override;
};
} // namespace rive

#endif