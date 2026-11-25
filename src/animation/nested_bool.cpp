#include "rive/animation/nested_bool.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/container_component.hpp"

using namespace rive;
class StateMachineInstance;

// Use the NestedBoolBase m_NestedValue on initialization but then it won't
// be used anymore and interface directly with the nested input value.
void NestedBool::applyValue()
{
  auto inputInstance = input();
  if (inputInstance != nullptr)
  {
    auto boolInput = static_cast<SMIBool*>(inputInstance);
    if (boolInput != nullptr)
    {
      boolInput->value(NestedBoolBase::nestedValue());
    }
  }
}

void NestedBool::nestedValue(bool value)
{
  auto inputInstance = input();
  if (inputInstance != nullptr)
  {
    auto boolInput = static_cast<SMIBool*>(inputInstance);
    if (boolInput != nullptr && boolInput->value() != value)
    {
      boolInput->value(value);
    }
  }
  else
  {
    // No machine yet (its host defers instancing): held for applyValue,
    // which the machine runs when it is created.
    m_NestedValue = value;
  }
}

bool NestedBool::nestedValue() const
{
  auto inputInstance = input();
  if (inputInstance != nullptr)
  {
    auto boolInput = static_cast<SMIBool*>(inputInstance);
    if (boolInput != nullptr)
    {
      return boolInput->value();
    }
    return false;
  }
  return m_NestedValue;
}

void NestedBool::resetHeld(const NestedInput* source)
{
  if (source != nullptr && source->is<NestedBool>())
  {
    m_NestedValue = source->as<NestedBool>()->NestedBoolBase::nestedValue();
  }
}
