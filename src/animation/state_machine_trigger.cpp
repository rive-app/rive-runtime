#include "animation/state_machine_trigger.hpp"

using namespace rive;

void StateMachineTrigger::fire() { m_Fired = true; }

void StateMachineTrigger::reset() { m_Fired = false; }