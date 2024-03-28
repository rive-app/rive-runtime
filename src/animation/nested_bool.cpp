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
    }
    return false;
}