#include "rive/animation/nested_bool.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/container_component.hpp"

using namespace rive;
class StateMachineInstance;

void NestedBool::nestedValueChanged() { this->applyValue(); }

void NestedBool::applyValue()
{
    auto inputInstance = input();
    if (inputInstance != nullptr)
    {
        auto boolInput = static_cast<SMIBool*>(inputInstance);
        if (boolInput != nullptr)
        {
            boolInput->value(nestedValue());
        }
    }
}