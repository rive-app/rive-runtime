#include "rive/animation/state_machine_layer.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/generated/animation/state_machine_base.hpp"
#include "rive/animation/any_state.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/animation/exit_state.hpp"

using namespace rive;

StateMachineLayer::~StateMachineLayer()
{
    for (auto state : m_States)
    {
        delete state;
    }
}

StatusCode StateMachineLayer::onAddedDirty(CoreContext* context)
{
    StatusCode code;
    for (auto state : m_States)
    {
        if ((code = state->onAddedDirty(context)) != StatusCode::Ok)
        {
            return code;
        }
        switch (state->coreType())
        {
            case AnyState::typeKey:
                m_Any = state->as<AnyState>();
                break;
            case EntryState::typeKey:
                m_Entry = state->as<EntryState>();
                break;
            case ExitState::typeKey:
                m_Exit = state->as<ExitState>();
                break;
        }
    }
    if (m_Any == nullptr || m_Entry == nullptr || m_Exit == nullptr)
    {
        // The layer is corrupt, we must have all three of these states.
        return StatusCode::InvalidObject;
    }

    return StatusCode::Ok;
}

StatusCode StateMachineLayer::onAddedClean(CoreContext* context)
{
    StatusCode code;
    for (auto state : m_States)
    {
        if ((code = state->onAddedClean(context)) != StatusCode::Ok)
        {
            return code;
        }
    }

    return StatusCode::Ok;
}

void StateMachineLayer::addState(LayerState* state) { m_States.push_back(state); }

StatusCode StateMachineLayer::import(ImportStack& importStack)
{
    auto stateMachineImporter = importStack.latest<StateMachineImporter>(StateMachineBase::typeKey);
    if (stateMachineImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    // WOW -- we're handing off ownership of this!
    stateMachineImporter->addLayer(std::unique_ptr<StateMachineLayer>(this));
    return Super::import(importStack);
}
