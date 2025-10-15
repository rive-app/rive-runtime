#include "rive/generated/script_input_trigger_base.hpp"
#include "rive/script_input_trigger.hpp"

using namespace rive;

Core* ScriptInputTriggerBase::clone() const
{
    auto cloned = new ScriptInputTrigger();
    cloned->copy(*this);
    return cloned;
}
