#include "rive/animation/nested_number.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/container_component.hpp"

using namespace rive;
class StateMachineInstance;

// Use the NestedNumberBase m_NestedValue on initialization but then it won't
// be used anymore and interface directly with the nested input value.
void NestedNumber::applyValue()
{
  auto inputInstance = input();
  if (inputInstance != nullptr)
  {
    auto numInput = static_cast<SMINumber*>(inputInstance);
    if (numInput != nullptr)
    {
      numInput->value(NestedNumberBase::nestedValue());
    }
  }
}

void NestedNumber::nestedValue(float value)
{
  auto inputInstance = input();
  if (inputInstance != nullptr)
  {
    auto numInput = static_cast<SMINumber*>(inputInstance);
    if (numInput != nullptr && numInput->value() != value)
    {
      numInput->value(value);
    }
  }
  else
  {
    // No machine yet (its host defers instancing): held for applyValue,
    // which the machine runs when it is created.
    m_NestedValue = value;
  }
}

float NestedNumber::nestedValue() const
{
  auto inputInstance = input();
  if (inputInstance != nullptr)
  {
    auto numInput = static_cast<SMINumber*>(inputInstance);
    if (numInput != nullptr)
    {
      return numInput->value();
    }
    return 0.0;
  }
  return m_NestedValue;
}

void NestedNumber::resetHeld(const NestedInput* source)
{
  if (source != nullptr && source->is<NestedNumber>())
  {
    m_NestedValue = source->as<NestedNumber>()->NestedNumberBase::nestedValue();
  }
}
