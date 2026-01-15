#include "rive/generated/animation/scripted_listener_action_base.hpp"
#include "rive/animation/scripted_listener_action.hpp"

using namespace rive;

Core* ScriptedListenerActionBase::clone() const
{
    auto cloned = new ScriptedListenerAction();
    cloned->copy(*this);
    return cloned;
}
