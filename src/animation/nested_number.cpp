#include "rive/animation/nested_number.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/container_component.hpp"

using namespace rive;
class StateMachineInstance;

void NestedNumber::nestedValueChanged() { this->applyValue(); }

void NestedNumber::applyValue()
{
    auto inputInstance = input();
    if (inputInstance != nullptr)
    {
        auto numInput = static_cast<SMINumber*>(inputInstance);
        if (numInput != nullptr)
        {
            numInput->value(nestedValue());
        }
    }
}