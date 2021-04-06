#include "importers/layer_state_importer.hpp"
#include "animation/state_transition.hpp"
#include "animation/layer_state.hpp"

using namespace rive;

LayerStateImporter::LayerStateImporter(LayerState* state) : m_State(state) {}
void LayerStateImporter::addTransition(StateTransition* transition)
{
	m_State->addTransition(transition);
}

StatusCode LayerStateImporter::resolve() { return StatusCode::Ok; }