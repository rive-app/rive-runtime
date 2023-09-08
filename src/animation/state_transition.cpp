#include "rive/animation/animation_state_instance.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/cubic_interpolator.hpp"
#include "rive/animation/layer_state.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/animation/transition_condition.hpp"
#include "rive/animation/transition_trigger_condition.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/layer_state_importer.hpp"

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

    if (interpolatorId() != -1)
    {
        auto coreObject = context->resolve(interpolatorId());
        if (coreObject == nullptr || !coreObject->is<CubicInterpolator>())
        {
            return StatusCode::MissingObject;
        }
        m_Interpolator = coreObject->as<CubicInterpolator>();
    }

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
    auto stateImporter = importStack.latest<LayerStateImporter>(LayerState::typeKey);
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
    if ((transitionFlags() & StateTransitionFlags::DurationIsPercentage) ==
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

float StateTransition::exitTimeSeconds(const LayerState* stateFrom, bool absolute) const
{
    if ((transitionFlags() & StateTransitionFlags::ExitTimeIsPercentage) ==
        StateTransitionFlags::ExitTimeIsPercentage)
    {
        float animationDuration = 0.0f;
        float start = 0.0f;

        auto exitAnimation = exitTimeAnimation(stateFrom);
        if (exitAnimation != nullptr)
        {
            // TODO: needs a looking for speed
            start = absolute ? exitAnimation->startSeconds() : 0.0f;
            animationDuration = exitAnimation->durationSeconds();
        }

        return start + exitTime() / 100.0f * animationDuration;
    }
    return exitTime() / 1000.0f;
}

const LinearAnimationInstance* StateTransition::exitTimeAnimationInstance(
    const StateInstance* from) const
{
    return from != nullptr && from->state()->is<AnimationState>()
               ? static_cast<const AnimationStateInstance*>(from)->animationInstance()
               : nullptr;
}

const LinearAnimation* StateTransition::exitTimeAnimation(const LayerState* from) const
{
    return from != nullptr && from->is<AnimationState>() ? from->as<AnimationState>()->animation()
                                                         : nullptr;
}

AllowTransition StateTransition::allowed(StateInstance* stateFrom,
                                         StateMachineInstance* stateMachineInstance,
                                         bool ignoreTriggers) const
{
    if (isDisabled())
    {
        return AllowTransition::no;
    }

    for (auto condition : m_Conditions)
    {
        // N.B. state machine instance sanitizes these for us...
        auto input = stateMachineInstance->input(condition->inputId());

        if ((ignoreTriggers && condition->is<TransitionTriggerCondition>()) ||
            !condition->evaluate(input))
        {
            return AllowTransition::no;
        }
    }

    if (enableExitTime())
    {
        auto exitAnimation = exitTimeAnimationInstance(stateFrom);
        if (exitAnimation != nullptr)
        {
            // Exit time is specified in a value less than a single loop, so we
            // want to allow exiting regardless of which loop we're on. To do
            // that we bring the exit time up to the loop our lastTime is at.
            auto lastTime = exitAnimation->lastTotalTime();
            auto time = exitAnimation->totalTime();
            auto exitTime = exitTimeSeconds(stateFrom->state());
            auto animationFrom = exitAnimation->animation();
            auto duration = animationFrom->durationSeconds();

            // TODO: there are some considerations to have when exit time is
            // combined with another condition (like trigger)
            //   - not sure how to get this to make sense with pingPing
            //   animations
            //   - also if exit time is, say 50% on a loop, this will be happy
            //   to fire
            //       - when time is anywhere in 50%-100%, 150%-200%. as opposed
            //       to just at 50%
            //       .... makes you wonder if we need some kind of exit
            //       after/exit before time
            //       .... but i suspect that will introduce some more issues?

            // There's only one iteration in oneShot,
            if (exitTime <= duration && animationFrom->loop() != Loop::oneShot)
            {
                // Get exit time relative to the loop lastTime was in.
                exitTime += std::floor(lastTime / duration) * duration;
            }

            // TODO: needs a looking for speed
            if (time < exitTime)
            {
                return AllowTransition::waitingForExit;
            }
        }
    }
    return AllowTransition::yes;
}

bool StateTransition::applyExitCondition(StateInstance* from) const
{
    // Hold exit time when the user has set to pauseOnExit on this condition
    // (only valid when exiting from an Animation).
    bool useExitTime = enableExitTime() && (from != nullptr && from->state()->is<AnimationState>());
    if (pauseOnExit() && useExitTime)
    {
        static_cast<AnimationStateInstance*>(from)->animationInstance()->time(
            exitTimeSeconds(from->state(), true));
        return true;
    }
    return useExitTime;
}