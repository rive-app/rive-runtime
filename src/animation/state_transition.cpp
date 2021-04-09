#include "animation/state_transition.hpp"
#include "importers/import_stack.hpp"
#include "importers/layer_state_importer.hpp"
#include "animation/layer_state.hpp"
#include "animation/transition_condition.hpp"
#include "animation/animation_state.hpp"
#include "animation/linear_animation.hpp"

using namespace rive;

StateTransition::~StateTransition()
{
	for (auto condition : m_Conditions)
	{
		delete condition;
	}
}

StatusCode StateTransition::onAddedDirty(CoreContext* context)
{
	StatusCode code;
	for (auto condition : m_Conditions)
	{
		if ((code = condition->onAddedDirty(context)) != StatusCode::Ok)
		{
			return code;
		}
	}
	return StatusCode::Ok;
}

StatusCode StateTransition::onAddedClean(CoreContext* context)
{
	StatusCode code;
	for (auto condition : m_Conditions)
	{
		if ((code = condition->onAddedClean(context)) != StatusCode::Ok)
		{
			return code;
		}
	}
	return StatusCode::Ok;
}

StatusCode StateTransition::import(ImportStack& importStack)
{
	auto stateImporter =
	    importStack.latest<LayerStateImporter>(LayerState::typeKey);
	if (stateImporter == nullptr)
	{
		return StatusCode::MissingObject;
	}
	stateImporter->addTransition(this);
	return Super::import(importStack);
}

void StateTransition::addCondition(TransitionCondition* condition)
{
	m_Conditions.push_back(condition);
}

float StateTransition::mixTime(const LayerState* stateFrom) const
{
	if (duration() == 0)
	{
		return 0;
	}
	if ((transitionFlags() & StateTransitionFlags::DurationIsPercentage) !=
	    StateTransitionFlags::DurationIsPercentage)
	{
		float animationDuration = 0.0f;
		if (stateFrom->is<AnimationState>())
		{
			auto animation = stateFrom->as<AnimationState>()->animation();
			if (animation != nullptr)
			{
				animationDuration = animation->durationSeconds();
			}
		}
		return duration() / 100.0f * animationDuration;
	}
	else
	{
		return duration() / 1000.0f;
	}
}

float StateTransition::exitTimeSeconds(const LayerState* stateFrom,
                                       bool relativeToWorkArea) const
{
	auto exitValue = exitTime();
	if (exitValue == 0)
	{
		return 0;
	}

	float animationDuration = 0.0f;
	float animationOrigin = 0.0f;
	if (stateFrom->is<AnimationState>())
	{
		auto animation = stateFrom->as<AnimationState>()->animation();
		animationDuration = animation->durationSeconds();
		animationOrigin = relativeToWorkArea ? 0 : animation->startSeconds();
	}

	if ((transitionFlags() & StateTransitionFlags::ExitTimeIsPercentage) ==
	    StateTransitionFlags::ExitTimeIsPercentage)
	{
		return animationOrigin + exitValue / 100.0f * animationDuration;
	}
	else
	{
		return animationOrigin + exitValue / 1000.0f;
	}
}
