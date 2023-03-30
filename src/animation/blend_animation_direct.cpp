#include "rive/animation/blend_animation_direct.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/importers/state_machine_importer.hpp"

using namespace rive;

StatusCode BlendAnimationDirect::onAddedDirty(CoreContext* context) { return StatusCode::Ok; }

StatusCode BlendAnimationDirect::onAddedClean(CoreContext* context) { return StatusCode::Ok; }

StatusCode BlendAnimationDirect::import(ImportStack& importStack)
{
    auto stateMachineImporter = importStack.latest<StateMachineImporter>(StateMachine::typeKey);
    if (stateMachineImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }

    // Make sure the inputId doesn't overflow the input buffer.
    if (blendSource() == static_cast<int>(DirectBlendSource::inputId))
    {
        if ((size_t)inputId() >= stateMachineImporter->stateMachine()->inputCount())
        {
            return StatusCode::InvalidObject;
        }
        auto input = stateMachineImporter->stateMachine()->input((size_t)inputId());
        if (input == nullptr || !input->is<StateMachineNumber>())
        {
            return StatusCode::InvalidObject;
        }
    }
    return Super::import(importStack);
}
