#include "rive/generated/script_input_component_base.hpp"
#include "rive/script_input_component.hpp"

using namespace rive;

Core* ScriptInputComponentBase::clone() const
{
  auto cloned = new ScriptInputComponent();
  cloned->copy(*this);
  return cloned;
}
