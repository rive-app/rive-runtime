#include "rive/animation/layer_state.hpp"
#include "rive/animation/transition_bool_condition.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/state_machine_layer_importer.hpp"
#include "rive/generated/animation/state_machine_layer_base.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/animation/system_state_instance.hpp"

using namespace rive;

LayerState::~LayerState()
{
    for (auto transition : m_Transitions)
    {
        delete transition;
    }
}

StatusCode LayerState::onAddedDirty(CoreContext* context)
{
    StatusCode code;
    for (auto transition : m_Transitions)
    {
        if ((code = transition->onAddedDirty(context)) != StatusCode::Ok)
        {
            return code;
        }
    }
    return StatusCode::Ok;
}

StatusCode LayerState::onAddedClean(CoreContext* context)
{
    StatusCode code;
    for (auto transition : m_Transitions)
    {
        if ((code = transition->onAddedClean(context)) != StatusCode::Ok)
        {
            return code;
        }
    }
    return StatusCode::Ok;
}

StatusCode LayerState::import(ImportStack& importStack)
{
    auto layerImporter =
        importStack.latest<StateMachineLayerImporter>(StateMachineLayerBase::typeKey);
    if (layerImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    layerImporter->addState(this);
    return Super::import(importStack);
}

void LayerState::addTransition(StateTransition* transition) { m_Transitions.push_back(transition); }

std::unique_ptr<StateInstance> LayerState::makeInstance(ArtboardInstance* instance) const
{
    return rivestd::make_unique<SystemStateInstance>(this, instance);
}