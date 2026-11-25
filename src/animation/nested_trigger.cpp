#include "rive/animation/nested_trigger.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/container_component.hpp"

using namespace rive;
class StateMachineInstance;

void NestedTrigger::fire(const CallbackData& value) { this->applyValue(); }

void NestedTrigger::applyValue()
{
  auto inputInstance = input();
  if (inputInstance != nullptr)
  {
    auto numInput = static_cast<SMITrigger*>(inputInstance);
    if (numInput != nullptr)
    {
      numInput->fire();
    }
  }
  else
  {
    // No machine yet: a collapsed one would hold the trigger until it next
    // advances, so this holds it until the machine is created.
    m_pendingFire = true;
  }
}

void NestedTrigger::resetHeld(const NestedInput* source)
{
  m_pendingFire = false;
}

void NestedTrigger::firePending()
{
  if (!m_pendingFire)
  {
    return;
  }
  m_pendingFire = false;
  applyValue();
}