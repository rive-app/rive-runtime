#include "rive/animation/state_machine_input.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/generated/animation/state_machine_base.hpp"

using namespace rive;

StatusCode StateMachineInput::onAddedDirty(CoreContext* context) { return StatusCode::Ok; }

StatusCode StateMachineInput::onAddedClean(CoreContext* context) { return StatusCode::Ok; }

StatusCode StateMachineInput::import(ImportStack& importStack)
{
    auto stateMachineImporter = importStack.latest<StateMachineImporter>(StateMachineBase::typeKey);
    if (stateMachineImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    // WOW -- we're handing off ownership of this!
    stateMachineImporter->addInput(std::unique_ptr<StateMachineInput>(this));
    return Super::import(importStack);
}