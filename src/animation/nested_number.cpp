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
    }
    return 0.0;
}