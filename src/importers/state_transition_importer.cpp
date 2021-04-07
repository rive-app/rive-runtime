#include "importers/state_transition_importer.hpp"
#include "animation/transition_condition.hpp"
#include "animation/state_transition.hpp"

using namespace rive;

StateTransitionImporter::StateTransitionImporter(StateTransition* transition) :
    m_Transition(transition)
{
}
void StateTransitionImporter::addCondition(TransitionCondition* condition)
{
	m_Transition->addCondition(condition);
}

StatusCode StateTransitionImporter::resolve() { return StatusCode::Ok; }