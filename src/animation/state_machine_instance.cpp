#include "rive/animation/animation_reset.hpp"
#include "rive/animation/animation_reset_factory.hpp"
#include "rive/animation/animation_state_instance.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/any_state.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/data_bind/bindable_property_number.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/animation/exit_state.hpp"
#include "rive/animation/layer_state_flags.hpp"
#include "rive/animation/nested_linear_animation.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/scripted_transition_condition.hpp"
#include "rive/animation/state_instance.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_input.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_instance_clusters.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/animation/state_machine_listener_single.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/animation/listener_action.hpp"
#include "rive/animation/listener_types/listener_input_type_viewmodel.hpp"
#include "rive/animation/scripted_listener_action.hpp"
#include "rive/animation/transition_condition.hpp"
#include "rive/animation/transition_comparator.hpp"
#include "rive/animation/transition_property_viewmodel_comparator.hpp"
#include "rive/animation/transition_viewmodel_condition.hpp"
#include "rive/animation/state_machine_fire_event.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/constraints/draggable_constraint.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind_flags.hpp"
#include "rive/event_report.hpp"
#include "rive/hit_result.hpp"
#include "rive/listener_group.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/random.hpp"
#include "rive/math/hit_test.hpp"
#include "rive/nested_animation.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/process_event_result.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/text/text.hpp"
#include "rive/math/math_types.hpp"
#include "rive/audio_event.hpp"
#include "rive/dirtyable.hpp"
#include "rive/profiler/profiler_macros.h"
#include "rive/text/text_input.hpp"
#include "rive/refcnt.hpp"
#include "rive/animation/focus_listener_group.hpp"
#include "rive/animation/text_input_listener_group.hpp"
#include "rive/animation/listener_types/listener_input_type_event.hpp"
#include "rive/focus_data.hpp"
#include "rive/node.hpp"
#include "rive/semantic/semantic_data.hpp"
#include "rive/view_model_type.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/file.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/viewmodel/write_attribution.hpp"
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <cmath>

using namespace rive;

// ArtboardComponentList builds one StateMachineInstance per row, so a 1000-row
// list pays sizeof(StateMachineInstance) a thousand times over before any
// content exists. The clusters in state_machine_instance_clusters.hpp exist to
// keep it small: 1080 B before that work, 368 B after, which the allocator
// rounds to 384 instead of 1280.
//
// Before adding an inline member, check whether it belongs in one of the
// SMI* sidecar clusters instead — anything that is only populated for a
// specific authored feature (events, bindables, focus/keyboard/gamepad/
// semantics, scripting) does. Note also that nothing inline here is a
// std::unordered_map any more, which is what makes this type the same size on
// libc++ and libstdc++; an inline hash container would give that up.

#ifdef RIVE_MICROPROFILE
#include "rive/profiler/rive_profile.hpp"
static std::string getStateName(const StateInstance* stateInstance)
{
    if (stateInstance == nullptr)
    {
        return "(null)";
    }
    auto state = stateInstance->state();
    if (state->is<AnimationState>())
    {
        auto anim = state->as<AnimationState>()->animation();
        return anim != nullptr ? anim->name() : "Animation";
    }
    if (state->is<EntryState>())
    {
        return "Entry";
    }
    if (state->is<ExitState>())
    {
        return "Exit";
    }
    if (state->is<AnyState>())
    {
        return "Any";
    }
    return "Blend";
}
#endif

namespace rive
{

class StateMachineLayerInstance
{
public:
    StateMachineLayerInstance() :
        m_transitionCompleted(false),
        m_holdAnimationFrom(false),
        m_stateMachineChangedOnAdvance(false),
        m_waitingForExit(false),
        m_settled(false)
    {}

    ~StateMachineLayerInstance()
    {
        delete m_anyStateInstance;
        delete m_currentState;
        delete m_stateFrom;
    }

    /// The artboard every layer of this instance applies to. This is
    /// identical for all layers of a given StateMachineInstance — as was the
    /// owning instance pointer — so holding either per layer stored the same
    /// value layerCount times over. Both are therefore derived from the `smi`
    /// threaded through the methods below rather than stored per layer.
    ///
    /// The layer *definition* is deliberately NOT derived this way. It is
    /// genuinely per-index data, and while `m_machine->layer(this - m_layers)`
    /// would recover it, that lookup is only stable in runtime builds. Under
    /// WITH_RIVE_EDITOR, StateMachine::layer() reads `m_editorLayers`, which
    /// EditorFile::finalizeBatch clears and rebuilds from arena order after
    /// every coop batch, while clearStalePlaybackScenes only rebuilds the
    /// StateMachineInstance when the StateMachine *pointer* changes. So adding
    /// or reparenting a layer can leave slot i resolving to a different
    /// definition — or, if a layer was deleted, to nullptr — while m_layers[i]
    /// still holds the old layer's runtime state. m_layer is captured once at
    /// init and pinned for the instance's lifetime instead.
    static ArtboardInstance* artboardOf(const StateMachineInstance* smi)
    {
        return smi->m_artboardInstance;
    }

    void init(StateMachineInstance* smi, const StateMachineLayer* layer)
    {
        assert(m_layer == nullptr);
        m_layer = layer;
        changeState(smi, m_layer->entryState());
    }

    void resetState(StateMachineInstance* smi)
    {
        // The current state goes away below even when changeState then finds
        // nothing to change to (no entry state).
        m_settled = false;
        if (m_stateFrom != m_anyStateInstance && m_stateFrom != m_currentState)
        {
            delete m_stateFrom;
        }
        m_stateFrom = nullptr;
        if (m_currentState != m_anyStateInstance)
        {
            delete m_currentState;
        }
        m_currentState = nullptr;
        changeState(smi, m_layer->entryState());
    }

    void updateMix(StateMachineInstance* smi, float seconds)
    {
        if (m_transition != nullptr && m_stateFrom != nullptr &&
            resolvedDuration() != 0)
        {
            auto mixTime = resolvedMixTime();
            if (mixTime == 0.0f)
            {
                m_mix = 1.0f;
            }
            else
            {
                m_mix =
                    std::min(1.0f, std::max(0.0f, (m_mix + seconds / mixTime)));
            }
            if (m_mix == 1.0f && !m_transitionCompleted)
            {
                m_transitionCompleted = true;
                clearAnimationReset();
                fireEvents(smi,
                           StateMachineFireOccurance::atEnd,
                           m_transition->events());
                performListenerActions(smi,
                                       StateMachineFireOccurance::atEnd,
                                       m_transition->listenerActions());
            }
        }
        else
        {
            m_mix = 1.0f;
        }
    }

    // A settled layer that has finished mixing, in a state that keys nothing,
    // has stopped keeping itself going and has no exit time to wait on: its
    // advance would only move a clock nothing reads, apply nothing and skip a
    // search that can't pass, so it returns what that advance would have.
    bool isFrozen() const
    {
        if (!m_settled || m_mix != 1.0f || m_animationReset != nullptr ||
            m_holdAnimation != nullptr ||
            (m_transition != nullptr && !m_transitionCompleted) ||
            m_currentState == nullptr || m_currentState->keepGoing())
        {
            return false;
        }
        auto state = m_currentState->state();
        if (!state->transitionsIgnoreTime())
        {
            return false;
        }
        // Checked on the instance rather than classified with the state: a
        // state's animation may not be resolved yet when that runs.
        switch (state->coreType())
        {
            case EntryStateBase::typeKey:
            case ExitStateBase::typeKey:
            case AnyStateBase::typeKey:
                return true;
            case AnimationStateBase::typeKey:
                // An animation state without an animation plays an empty one.
                return static_cast<const AnimationStateInstance*>(
                           m_currentState)
                           ->animationInstance()
                           ->animation()
                           ->numKeyedObjects() == 0;
            default:
                return false;
        }
    }

    bool advance(StateMachineInstance* smi, float seconds, bool newFrame)
    {
        RIVE_WRITE_SOURCE(layer, m_layer, smi);
        if (newFrame)
        {
            m_stateMachineChangedOnAdvance = false;
        }
        if (isFrozen())
        {
#ifdef TESTING
            StateMachineInstance::sm_frozenLayerAdvances++;
#endif
            return false;
        }
        m_currentState->advance(seconds, smi);
        updateMix(smi, seconds);

        if (m_stateFrom != nullptr && m_mix < 1.0f && !m_holdAnimationFrom)
        {
            // This didn't advance during our updateState, but it should now
            // that we realize we need to mix it in.
            m_stateFrom->advance(seconds, smi);
        }

        apply(smi);

        bool changedState = false;

        for (int i = 0; updateState(smi); i++)
        {
            changedState = true;
            apply(smi);

            if (i == maxIterations)
            {
                auto stateMachineName =
                    smi->stateMachine() == nullptr
                        ? "[SM Not found]"
                        : smi->stateMachine()->name().c_str();
                auto layerName = m_layer == nullptr ? "[LY Not found]"
                                                    : m_layer->name().c_str();
                auto artboardName = smi->artboard() == nullptr
                                        ? "[AB Not found]"
                                        : smi->artboard()->name().c_str();
                fprintf(stderr,
                        "%s StateMachine exceeded max iterations in layer %s "
                        "on artboard %s\n",
                        stateMachineName,
                        layerName,
                        artboardName);
                return false;
            }
        }

        m_currentState->clearSpilledTime();

        return changedState || m_mix != 1.0f || m_waitingForExit ||
               (m_currentState != nullptr && m_currentState->keepGoing());
    }

    /// Returns the per-instance transition duration, resolving any data
    /// binding override. Falls back to the shared definition value when
    /// no binding exists.
    uint32_t resolvedDuration() const
    {
        if (m_transitionDurationProperty != nullptr)
        {
            float val = m_transitionDurationProperty->propertyValue();
            return val < 0 ? 0 : static_cast<uint32_t>(std::round(val));
        }
        return m_transition->duration();
    }

    /// Computes the mix time using the per-instance resolved duration.
    float resolvedMixTime() const
    {
        auto dur = resolvedDuration();
        if (dur == 0)
        {
            return 0;
        }
        if (m_transition->durationIsPercentage())
        {
            float animationDuration = 0.0f;
            auto state = m_stateFrom->state();
            if (state->is<AnimationState>())
            {
                auto animation = state->as<AnimationState>()->animation();
                if (animation != nullptr)
                {
                    animationDuration = animation->durationSeconds();
                }
            }
            return (float)dur / 100.0f * animationDuration;
        }
        return (float)dur / 1000.0f;
    }

    bool isTransitioning()
    {
        return m_transition != nullptr && m_stateFrom != nullptr &&
               resolvedDuration() != 0 && m_mix < 1.0f;
    }

    /// The any state's instance is only ever fed to tryChangeState, so a layer
    /// whose any state has no transitions never needs one. Most don't, so this
    /// is built on demand instead of at init: it saves a heap allocation per
    /// layer in the common case. Lazy rather than a one-shot check at init
    /// because LayerState::transitionCount() also reports the editor's
    /// live-edit list, which can grow after this instance was built.
    void ensureAnyStateInstance(StateMachineInstance* smi)
    {
        if (m_anyStateInstance != nullptr)
        {
            return;
        }
        // A layer without an any state is degenerate but not fatal: every
        // other use of m_anyStateInstance is either a delete guard or a
        // tryChangeState call, both of which handle null. Keeping this
        // tolerant is what lets StateMachineLayer stop requiring the state
        // to be present, so exports can eventually omit unused ones.
        auto anyState = m_layer == nullptr ? nullptr : m_layer->anyState();
        if (anyState == nullptr || anyState->transitionCount() == 0)
        {
            return;
        }
        m_anyStateInstance = anyState->makeInstance(artboardOf(smi)).release();
    }

    bool updateState(StateMachineInstance* smi)
    {
        // Don't allow changing state while a transition is taking place
        // (we're mixing one state onto another) if enableEarlyExit is not true.
        if (isTransitioning() && !m_transition->enableEarlyExit())
        {
            return false;
        }

        m_waitingForExit = false;

        ensureAnyStateInstance(smi);
        if (m_settled)
        {
#ifdef TESTING
            verifySettled(smi);
            StateMachineInstance::sm_settledLayerSkips++;
#endif
            return false;
        }
        if (tryChangeState(smi, m_anyStateInstance))
        {
            return true;
        }
        if (tryChangeState(smi, m_currentState))
        {
            return true;
        }
#ifndef WITH_RIVE_EDITOR
        // Nothing to change to, and that holds until the current state changes
        // or a value these transitions read does, provided they read only
        // values whose changes reach this instance (transitionsSettleSafe),
        // none is waiting on an exit time, and no bind is waiting to be
        // applied (a condition may just have read a bindable's old value).
        // StateMachineInstance::unsettleLayers undoes it.
        if (!m_waitingForExit && !smi->m_bindsPending &&
            isSettleSafe(m_anyStateInstance) && isSettleSafe(m_currentState))
        {
            m_settled = true;
            smi->m_anySettled = true;
        }
#endif
        return false;
    }

    void unsettle() { m_settled = false; }

    static bool isSettleSafe(const StateInstance* stateInstance)
    {
        return stateInstance == nullptr ||
               stateInstance->state()->transitionsSettleSafe();
    }

#ifdef TESTING
    // A settled layer's search must still come up empty. Repeats it without
    // its side effects (random weights, used-layer marks, m_waitingForExit)
    // and stops loudly if the settled answer went stale.
    void verifySettled(StateMachineInstance* smi)
    {
        StateInstance* stateFromInstances[] = {m_anyStateInstance,
                                               m_currentState};
        for (StateInstance* stateFromInstance : stateFromInstances)
        {
            if (stateFromInstance == nullptr)
            {
                continue;
            }
            auto stateFrom = stateFromInstance->state();
            for (size_t i = 0, count = stateFrom->transitionCount(); i < count;
                 i++)
            {
                auto transition = stateFrom->transition(i);
                if (canChangeState(transition->stateTo()) &&
                    transition->allowed(stateFromInstance, smi, this) !=
                        AllowTransition::no)
                {
                    fprintf(stderr,
                            "Settled layer %s on artboard %s would have taken "
                            "a transition\n",
                            m_layer->name().c_str(),
                            smi->artboard()->name().c_str());
                    abort();
                }
            }
        }
    }
#endif

    void fireEvents(StateMachineInstance* smi,
                    StateMachineFireOccurance occurs,
                    const std::vector<StateMachineFireAction*>& fireEvents)
    {
        for (auto event : fireEvents)
        {
            if (event->occurs() == occurs)
            {
                event->perform(smi);
            }
        }
    }

    void performListenerActions(
        StateMachineInstance* smi,
        StateMachineFireOccurance occurs,
        const std::vector<std::unique_ptr<ListenerAction>>& listenerActions)
    {
        bool performed = false;
        for (const auto& action : listenerActions)
        {
            if (action->matchesScheduledOccurrence(occurs))
            {
                action->perform(smi, ListenerInvocation::none());
                performed = true;
            }
        }
        if (performed)
        {
            smi->updateDataBinds(false);
        }
    }

    bool canChangeState(const LayerState* stateTo)
    {
        return !(
            (m_currentState == nullptr ? nullptr : m_currentState->state()) ==
            stateTo);
    }

    double randomValue() { return RandomProvider::generateRandomFloat(); }

    void changeState(StateMachineInstance* smi, const LayerState* stateTo)
    {
        if ((m_currentState == nullptr ? nullptr : m_currentState->state()) ==
            stateTo)
        {
            return;
        }
        // A different state has different transitions to search.
        m_settled = false;

        // Fire end events for the state we're changing from.
        if (m_currentState != nullptr)
        {
            fireEvents(smi,
                       StateMachineFireOccurance::atEnd,
                       m_currentState->state()->events());
            performListenerActions(smi,
                                   StateMachineFireOccurance::atEnd,
                                   m_currentState->state()->listenerActions());
        }

        m_currentState = stateTo == nullptr
                             ? nullptr
                             : stateTo->makeInstance(artboardOf(smi)).release();

        // Fire start events for the state we're changing to.
        if (m_currentState != nullptr)
        {
            fireEvents(smi,
                       StateMachineFireOccurance::atStart,
                       m_currentState->state()->events());
            performListenerActions(smi,
                                   StateMachineFireOccurance::atStart,
                                   m_currentState->state()->listenerActions());
        }
        return;
    }

    StateTransition* findRandomTransition(StateMachineInstance* smi,
                                          StateInstance* stateFromInstance)
    {
        uint32_t totalWeight = 0;
        auto stateFrom = stateFromInstance->state();
        for (size_t i = 0, length = stateFrom->transitionCount(); i < length;
             i++)
        {
            auto transition = stateFrom->transition(i);
            if (canChangeState(transition->stateTo()))
            {

                auto allowed =
                    transition->allowed(stateFromInstance, smi, this);
                if (allowed == AllowTransition::yes)
                {
                    transition->evaluatedRandomWeight(
                        transition->randomWeight());
                    totalWeight += transition->randomWeight();
                }
                else
                {
                    transition->evaluatedRandomWeight(0);
                    if (allowed == AllowTransition::waitingForExit)
                    {
                        m_waitingForExit = true;
                    }
                }
            }
            else
            {
                transition->evaluatedRandomWeight(0);
            }
        }
        if (totalWeight > 0)
        {
            double randomWeight = randomValue() * totalWeight * 1.0;
            double currentWeight = 0;
            size_t index = 0;
            StateTransition* transition;
            while (index < stateFrom->transitionCount())
            {
                transition = stateFrom->transition(index);
                double transitionWeight =
                    (double)transition->evaluatedRandomWeight();
                if (currentWeight + transitionWeight > randomWeight)
                {
                    transition->useLayerInConditions(smi, this);
                    return transition;
                }
                currentWeight += transitionWeight;
                index++;
            }
        }
        return nullptr;
    }

    StateTransition* findAllowedTransition(StateMachineInstance* smi,
                                           StateInstance* stateFromInstance)
    {
        auto stateFrom = stateFromInstance->state();
        // If it should randomize
        if ((static_cast<LayerStateFlags>(stateFrom->flags()) &
             LayerStateFlags::Random) == LayerStateFlags::Random)
        {
            return findRandomTransition(smi, stateFromInstance);
        }
        // Else search the first valid transition
        for (size_t i = 0, length = stateFrom->transitionCount(); i < length;
             i++)
        {
            auto transition = stateFrom->transition(i);
            if (canChangeState(transition->stateTo()))
            {

                auto allowed =
                    transition->allowed(stateFromInstance, smi, this);
                if (allowed == AllowTransition::yes)
                {
                    transition->evaluatedRandomWeight(
                        transition->randomWeight());
                    transition->useLayerInConditions(smi, this);
                    return transition;
                }
                else
                {
                    transition->evaluatedRandomWeight(0);
                    if (allowed == AllowTransition::waitingForExit)
                    {
                        m_waitingForExit = true;
                    }
                }
            }
        }
        return nullptr;
    }

    void buildAnimationResetForTransition(StateMachineInstance* smi)
    {
        m_animationReset = AnimationResetFactory::fromStates(m_stateFrom,
                                                             m_currentState,
                                                             artboardOf(smi));
    }

    void clearAnimationReset()
    {
        if (m_animationReset != nullptr)
        {
            AnimationResetFactory::release(std::move(m_animationReset));
            m_animationReset = nullptr;
        }
    }

    bool tryChangeState(StateMachineInstance* smi,
                        StateInstance* stateFromInstance)
    {
        if (stateFromInstance == nullptr)
        {
            return false;
        }
        auto outState = m_currentState;
        auto transition = findAllowedTransition(smi, stateFromInstance);
        if (transition != nullptr)
        {
            clearAnimationReset();
            changeState(smi, transition->stateTo());
            m_stateMachineChangedOnAdvance = true;
#ifdef RIVE_MICROPROFILE
            RiveProfile::instance().recordTransition(
                smi->artboard()->name(),
                smi->name(),
                m_layer->name(),
                getStateName(outState),
                getStateName(m_currentState),
                smi->artboard());
#endif
            // state actually has changed
            m_transition = transition;
            m_transitionDurationProperty = smi->findTransitionPropertyInstance(
                transition,
                StateTransitionBase::durationPropertyKey);
            fireEvents(smi,
                       StateMachineFireOccurance::atStart,
                       transition->events());
            performListenerActions(smi,
                                   StateMachineFireOccurance::atStart,
                                   transition->listenerActions());
            if (resolvedDuration() == 0)
            {
                m_transitionCompleted = true;
                fireEvents(smi,
                           StateMachineFireOccurance::atEnd,
                           transition->events());
                performListenerActions(smi,
                                       StateMachineFireOccurance::atEnd,
                                       transition->listenerActions());
            }
            else
            {
                m_transitionCompleted = false;
            }

            if (m_stateFrom != m_anyStateInstance)
            {
                // Old state from is done.
                delete m_stateFrom;
            }
            m_stateFrom = outState;

            if (!m_transitionCompleted)
            {
                buildAnimationResetForTransition(smi);
            }

            // If we had an exit time and wanted to pause on exit, make
            // sure to hold the exit time. Delegate this to the
            // transition by telling it that it was completed.
            if (outState != nullptr && transition->applyExitCondition(outState))
            {
                // Make sure we apply this state. This only returns true
                // when it's an animation state instance.
                auto instance =
                    static_cast<AnimationStateInstance*>(m_stateFrom)
                        ->animationInstance();

                m_holdAnimation = instance->animation();
                m_holdTime = instance->time();
            }
            m_mixFrom = m_mix;

            // Keep mixing last animation that was mixed in.
            if (m_mix != 0.0f)
            {
                m_holdAnimationFrom = transition->pauseOnExit();
            }
            if (m_currentState != nullptr)
            {
                auto advanceTime = 0.0f;
                if (m_stateFrom != nullptr)
                {
                    if (m_stateFrom->state()->is<AnimationState>())
                    {

                        auto instance =
                            static_cast<AnimationStateInstance*>(m_stateFrom)
                                ->animationInstance();

                        advanceTime = instance->spilledTime();
                    }
                }
                m_currentState->advance(advanceTime, smi);
            }
            m_mix = 0.0f;
            updateMix(smi, 0.0f);
            m_waitingForExit = false;
            return true;
        }
        return false;
    }

    void apply(StateMachineInstance* smi)
    {
        auto artboardInstance = artboardOf(smi);
        if (m_animationReset != nullptr)
        {
            m_animationReset->apply(artboardInstance);
        }
        if (m_holdAnimation != nullptr)
        {
            m_holdAnimation->apply(artboardInstance, m_holdTime, m_mixFrom);
            m_holdAnimation = nullptr;
        }

        KeyFrameInterpolator* interpolator = nullptr;
        if (m_transition != nullptr && m_transition->interpolator() != nullptr)
        {
            interpolator = m_transition->interpolator();
        }

        if (m_stateFrom != nullptr && m_mix < 1.0f)
        {
            auto fromMix = interpolator != nullptr
                               ? interpolator->transform(m_mixFrom)
                               : m_mixFrom;
            m_stateFrom->apply(artboardInstance, fromMix);
        }
        if (m_currentState != nullptr)
        {
            auto mix = interpolator != nullptr ? interpolator->transform(m_mix)
                                               : m_mix;
            m_currentState->apply(artboardInstance, mix);
        }
    }

    bool stateChangedOnAdvance() const
    {
        return m_stateMachineChangedOnAdvance;
    }

    const LayerState* currentState()
    {
        return m_currentState == nullptr ? nullptr : m_currentState->state();
    }

    const LinearAnimationInstance* currentAnimation() const
    {
        if (m_currentState == nullptr ||
            !m_currentState->state()->is<AnimationState>())
        {
            return nullptr;
        }
        return static_cast<AnimationStateInstance*>(m_currentState)
            ->animationInstance();
    }

private:
    static const int maxIterations = 100;

    // One of these exists per layer of every StateMachineInstance, which in an
    // ArtboardComponentList means per layer per row. Keep the pointers, then
    // the floats, then the bools: interleaving them costs 8 B of padding for
    // nothing. The owning instance and its artboard used to be stored here
    // too; both are the same for every layer of an instance, so they are now
    // derived from the `smi` argument threaded through the methods above. The
    // layer definition stays stored — see artboardOf() for why deriving it
    // from the array index is not safe in editor builds.
    const StateMachineLayer* m_layer = nullptr;
    StateInstance* m_anyStateInstance = nullptr;
    StateInstance* m_currentState = nullptr;
    StateInstance* m_stateFrom = nullptr;

    const StateTransition* m_transition = nullptr;
    BindablePropertyNumber* m_transitionDurationProperty = nullptr;
    std::unique_ptr<AnimationReset> m_animationReset = nullptr;
    /// Used to ensure a specific animation is applied on the next apply.
    const LinearAnimation* m_holdAnimation = nullptr;

    float m_mix = 1.0f;
    float m_mixFrom = 1.0f;
    float m_holdTime = 0.0f;

    // One bit each, so m_settled shares the byte the four flags before it
    // already take and sizeof stays 80. Bit-fields can't have default member
    // initializers before C++20, so the constructor sets them.
    bool m_transitionCompleted : 1;
    bool m_holdAnimationFrom : 1;
    bool m_stateMachineChangedOnAdvance : 1;
    bool m_waitingForExit : 1;
    // The last search found nothing and nothing it read has changed since,
    // so the next one would find nothing too. See updateState.
    bool m_settled : 1;
};

/// Representation of a Component from the Artboard Instance and all the
/// listeners it triggers. Allows tracking hover and performing hit detection
/// only once on components that trigger multiple listeners.
class HitDrawable : public HitComponent
{
public:
    HitDrawable(Drawable* drawable,
                Component* component,
                StateMachineInstance* stateMachineInstance,
                bool isOpaque) :
        HitComponent(component, stateMachineInstance)
    {
        this->m_drawable = drawable;
        this->isOpaque = isOpaque;
        if (drawable->isTargetOpaque())
        {
            canEarlyOut = false;
        }
    }
    float hitRadius = 2;
    bool isHovered = false;
    bool canEarlyOut = true;
    bool hasDownListener = false;
    bool hasUpListener = false;
    bool isOpaque = false;
    Drawable* m_drawable;
    std::vector<ListenerGroup*> listeners;

    bool hitTest(Vec2D position) const override { return false; }

    DraggableProxy* scrollProxyFor(const ScrollEvent& event)
    {
        // A live gesture keeps the view it started on, even once that view
        // can't move; only then does an idle proxy get a chance at the delta.
        for (auto listenerGroup : listeners)
        {
            auto proxy = listenerGroup->scrollProxy();
            if (proxy != nullptr && proxy->isScrollGestureActive())
            {
                return proxy;
            }
        }
        for (auto listenerGroup : listeners)
        {
            auto proxy = listenerGroup->scrollProxy();
            if (proxy != nullptr && proxy->wantsScroll(event))
            {
                return proxy;
            }
        }
        return nullptr;
    }

    bool wantsScroll(Vec2D position, const ScrollEvent& event) override
    {
        return hitTest(position) && scrollProxyFor(event) != nullptr;
    }

    bool hasScrollTarget(Vec2D position) override
    {
        if (!hitTest(position))
        {
            return false;
        }
        for (auto listenerGroup : listeners)
        {
            auto proxy = listenerGroup->scrollProxy();
            if (proxy != nullptr && proxy->acceptsScroll())
            {
                return true;
            }
        }
        return false;
    }

    bool listensToButtonAt(Vec2D position, PointerButton button) override
    {
        if (!hitTest(position))
        {
            return false;
        }
        for (auto listenerGroup : listeners)
        {
            auto listener = listenerGroup->listener();
            if (listener != nullptr && listener->listensToButton(button))
            {
                return true;
            }
        }
        return false;
    }

    bool occludesPointer(Vec2D position) override
    {
        return (isOpaque || m_drawable->isTargetOpaque()) && hitTest(position);
    }

    bool occludesScroll(Vec2D position) override
    {
        // A scroll view never occludes: at its edge it declines and the
        // gesture chains outward.
        for (auto listenerGroup : listeners)
        {
            if (listenerGroup->scrollProxy() != nullptr)
            {
                return false;
            }
        }
        // Same opacity test as processEvent's hitOpaque.
        return (isOpaque || m_drawable->isTargetOpaque()) && hitTest(position);
    }

    bool scrollGestureActive() override
    {
        for (auto listenerGroup : listeners)
        {
            auto proxy = listenerGroup->scrollProxy();
            if (proxy != nullptr && proxy->isScrollGestureActive())
            {
                return true;
            }
        }
        return false;
    }

    void cancelScroll() override
    {
        for (auto listenerGroup : listeners)
        {
            auto proxy = listenerGroup->scrollProxy();
            if (proxy != nullptr)
            {
                proxy->cancelScroll();
            }
        }
    }

    HitResult processScroll(Vec2D position,
                            const ScrollEvent& event,
                            float timeStamp) override
    {
        auto proxy = scrollProxyFor(event);
        if (proxy == nullptr || !proxy->scroll(event, timeStamp))
        {
            return HitResult::none;
        }
        return isOpaque ? HitResult::hitOpaque : HitResult::hit;
    }

    void prepareEvent(Vec2D position,
                      ListenerType hitType,
                      int pointerId) override
    {
        if (canEarlyOut &&
            (hitType != ListenerType::down || !hasDownListener) &&
            (hitType != ListenerType::up || !hasUpListener))
        {
#ifdef TESTING
            earlyOutCount++;
#endif
            return;
        }
        isHovered = hitType != ListenerType::exit && hitTest(position);

        // // iterate all listeners associated with this hit shape
        if (isHovered)
        {
            for (auto listenerGroup : listeners)
            {

                listenerGroup->hover(pointerId);
            }
        }
    }

    HitResult processGamepadInvocation(
        const ListenerInvocation& invocation,
        ScriptedDrawable* alreadyDispatched) override
    {
        return HitResult::none;
    }

    HitResult processEvent(Vec2D position,
                           ListenerType hitType,
                           bool canHit,
                           float timeStamp,
                           int pointerId,
                           PointerButton button) override
    {
        // If the shape doesn't have any ListenerType::move / enter / exit and
        // the event being processed is not of the type it needs to handle.
        // There is no need to perform a hitTest (which is relatively expensive
        // and would be happening on every pointer move) so we early out.
        if (canEarlyOut &&
            (hitType != ListenerType::down || !hasDownListener) &&
            (hitType != ListenerType::up || !hasUpListener))
        {
            return HitResult::none;
        }
        bool isBlockingEvent = false;
        // // iterate all listeners associated with this hit shape
        for (auto listenerGroup : listeners)
        {
            if (listenerGroup->isConsumed())
            {
                continue;
            }
            if (listenerGroup->processEvent(m_component,
                                            position,
                                            pointerId,
                                            hitType,
                                            button,
                                            canHit,
                                            timeStamp,
                                            m_stateMachineInstance) ==
                ProcessEventResult::scroll)
            {
                isBlockingEvent = true;
            }
        }
        return (isHovered && canHit)
                   ? (isOpaque || m_drawable->isTargetOpaque() ||
                      isBlockingEvent)
                         ? HitResult::hitOpaque
                         : HitResult::hit
                   : HitResult::none;
    }

    void addListener(ListenerGroup* listenerGroup)
    {
        if (!listenerGroup->canEarlyOut(m_component))
        {
            canEarlyOut = false;
        }
        else
        {
            if (listenerGroup->needsDownListener(m_component))
            {
                hasDownListener = true;
            }
            if (listenerGroup->needsUpListener(m_component))
            {
                hasUpListener = true;
            }
        }
        listeners.push_back(listenerGroup);
    }

    void enablePointerEvents(int pointerId) override
    {
        for (auto listenerGroup : listeners)
        {
            listenerGroup->enable(pointerId);
        }
    }

    void disablePointerEvents(int pointerId) override
    {
        for (auto listenerGroup : listeners)
        {
            listenerGroup->disable(pointerId);
        }
    }
};

/// Representation of a HitDrawable with a Hittable component
class HitExpandable : public HitDrawable
{
public:
    HitExpandable(Drawable* drawable,
                  Component* component,
                  StateMachineInstance* stateMachineInstance,
                  bool isOpaque = false) :
        HitDrawable(drawable, component, stateMachineInstance, isOpaque)
    {}

    bool hitTest(Vec2D position) const override
    {
        return m_component->hitTestPoint(position, true, true);
    }
};

class HitTextRun : public HitExpandable
{
public:
    HitTextRun(Drawable* drawable,
               TextValueRun* component,
               StateMachineInstance* stateMachineInstance,
               bool isOpaque = false) :
        HitExpandable(drawable, component, stateMachineInstance, isOpaque)
    {
        if (component)
        {
            component->isHitTarget(true);
        }
    }
};

class HitLayout : public HitDrawable
{
public:
    HitLayout(Drawable* layout,
              StateMachineInstance* stateMachineInstance,
              bool isOpaque = false) :
        HitDrawable(layout, layout, stateMachineInstance, isOpaque)
    {}

    bool hitTest(Vec2D position) const override
    {
        return m_component->hitTestPoint(position, false, true);
    }
};

class HitNestedArtboard : public HitComponent
{
public:
    HitNestedArtboard(Component* nestedArtboard,
                      StateMachineInstance* stateMachineInstance) :
        HitComponent(nestedArtboard, stateMachineInstance)
    {}
    ~HitNestedArtboard() override {}

    bool hitTest(Vec2D position) const override
    {
        auto nestedArtboard = m_component->as<NestedArtboard>();
        if (nestedArtboard->isCollapsed() || nestedArtboard->isHidden() ||
            nestedArtboard->isPaused())
        {
            return false;
        }
        Vec2D nestedPosition;
        if (!nestedArtboard->worldToLocal(position, &nestedPosition))
        {
            // Mounted artboard isn't ready or has a 0 scale transform.
            return false;
        }

        for (auto nestedAnimation : nestedArtboard->nestedAnimations())
        {
            if (nestedAnimation->is<NestedStateMachine>())
            {
                auto nestedStateMachine =
                    nestedAnimation->as<NestedStateMachine>();
                if (nestedStateMachine->hitTest(nestedPosition))
                {
                    return true;
                }
            }
        }
        return false;
    }
    bool hitTestBounded(Vec2D position) const override
    {
        auto nestedArtboard = m_component->as<NestedArtboard>();
        if (nestedArtboard->isCollapsed() || nestedArtboard->isHidden() ||
            nestedArtboard->isPaused())
        {
            return false;
        }
        Vec2D nestedPosition;
        if (!nestedArtboard->worldToLocal(position, &nestedPosition))
        {
            return false;
        }
        for (auto nestedAnimation : nestedArtboard->nestedAnimations())
        {
            if (nestedAnimation->is<NestedStateMachine>() &&
                nestedAnimation->as<NestedStateMachine>()->hitTestBounded(
                    nestedPosition))
            {
                return true;
            }
        }
        return false;
    }
    /// Maps a parent-space delta into the nested artboard. A delta is a
    /// vector, so it takes the mapping's linear part only.
    bool nestedDelta(NestedArtboard* nestedArtboard,
                     Vec2D position,
                     Vec2D nestedPosition,
                     Vec2D delta,
                     Vec2D& out)
    {
        Vec2D shifted;
        if (!nestedArtboard->worldToLocal(position + delta, &shifted))
        {
            return false;
        }
        out = shifted - nestedPosition;
        return true;
    }

    bool wantsScroll(Vec2D position, const ScrollEvent& event) override
    {
        auto nestedArtboard = m_component->as<NestedArtboard>();
        if (nestedArtboard->isCollapsed() || nestedArtboard->isPaused())
        {
            return false;
        }
        Vec2D nestedPosition;
        if (!nestedArtboard->worldToLocal(position, &nestedPosition))
        {
            return false;
        }
        ScrollEvent mapped = event;
        if (!nestedDelta(nestedArtboard,
                         position,
                         nestedPosition,
                         event.delta,
                         mapped.delta))
        {
            return false;
        }
        for (auto nestedAnimation : nestedArtboard->nestedAnimations())
        {
            if (nestedAnimation->is<NestedStateMachine>() &&
                nestedAnimation->as<NestedStateMachine>()->wantsScroll(
                    nestedPosition,
                    mapped))
            {
                return true;
            }
        }
        return false;
    }

    bool hasScrollTarget(Vec2D position) override
    {
        auto nestedArtboard = m_component->as<NestedArtboard>();
        if (nestedArtboard->isCollapsed() || nestedArtboard->isPaused())
        {
            return false;
        }
        Vec2D nestedPosition;
        if (!nestedArtboard->worldToLocal(position, &nestedPosition))
        {
            return false;
        }
        for (auto nestedAnimation : nestedArtboard->nestedAnimations())
        {
            if (nestedAnimation->is<NestedStateMachine>() &&
                nestedAnimation->as<NestedStateMachine>()->hasScrollTargetAt(
                    nestedPosition))
            {
                return true;
            }
        }
        return false;
    }

    bool listensToButtonAt(Vec2D position, PointerButton button) override
    {
        auto nestedArtboard = m_component->as<NestedArtboard>();
        if (nestedArtboard->isCollapsed() || nestedArtboard->isHidden() ||
            nestedArtboard->isPaused())
        {
            return false;
        }
        Vec2D nestedPosition;
        if (!nestedArtboard->worldToLocal(position, &nestedPosition))
        {
            return false;
        }
        for (auto nestedAnimation : nestedArtboard->nestedAnimations())
        {
            if (nestedAnimation->is<NestedStateMachine>() &&
                nestedAnimation->as<NestedStateMachine>()->listensToButtonAt(
                    nestedPosition,
                    button))
            {
                return true;
            }
        }
        return false;
    }

    bool occludesScroll(Vec2D position) override
    {
        auto nestedArtboard = m_component->as<NestedArtboard>();
        if (nestedArtboard->isCollapsed() || nestedArtboard->isPaused())
        {
            return false;
        }
        Vec2D nestedPosition;
        if (!nestedArtboard->worldToLocal(position, &nestedPosition))
        {
            return false;
        }
        for (auto nestedAnimation : nestedArtboard->nestedAnimations())
        {
            if (nestedAnimation->is<NestedStateMachine>() &&
                nestedAnimation->as<NestedStateMachine>()->scrollOccludedAt(
                    nestedPosition))
            {
                return true;
            }
        }
        return false;
    }

    bool scrollGestureActive() override
    {
        auto nestedArtboard = m_component->as<NestedArtboard>();
        if (nestedArtboard->isCollapsed() || nestedArtboard->isPaused())
        {
            // A paused machine never advances, so its idle timer can never
            // clear the latch. End it outright, or unpausing resurrects it.
            for (auto nestedAnimation : nestedArtboard->nestedAnimations())
            {
                if (nestedAnimation->is<NestedStateMachine>())
                {
                    nestedAnimation->as<NestedStateMachine>()->cancelScroll();
                }
            }
            return false;
        }
        for (auto nestedAnimation : nestedArtboard->nestedAnimations())
        {
            if (nestedAnimation->is<NestedStateMachine>() &&
                nestedAnimation->as<NestedStateMachine>()->hasScrollLatch())
            {
                return true;
            }
        }
        return false;
    }

    HitResult processScroll(Vec2D position,
                            const ScrollEvent& event,
                            float timeStamp) override
    {
        auto nestedArtboard = m_component->as<NestedArtboard>();
        Vec2D nestedPosition;
        if (!nestedArtboard->worldToLocal(position, &nestedPosition))
        {
            return HitResult::none;
        }
        ScrollEvent mapped = event;
        if (!nestedDelta(nestedArtboard,
                         position,
                         nestedPosition,
                         event.delta,
                         mapped.delta))
        {
            return HitResult::none;
        }
        // A latched machine gets the event first; only then can another
        // take it.
        for (auto nestedAnimation : nestedArtboard->nestedAnimations())
        {
            if (nestedAnimation->is<NestedStateMachine>() &&
                nestedAnimation->as<NestedStateMachine>()->hasScrollLatch())
            {
                return nestedAnimation->as<NestedStateMachine>()->pointerScroll(
                    nestedPosition,
                    mapped,
                    timeStamp);
            }
        }
        for (auto nestedAnimation : nestedArtboard->nestedAnimations())
        {
            if (!nestedAnimation->is<NestedStateMachine>())
            {
                continue;
            }
            auto result =
                nestedAnimation->as<NestedStateMachine>()->pointerScroll(
                    nestedPosition,
                    mapped,
                    timeStamp);
            if (result != HitResult::none)
            {
                return result;
            }
        }
        return HitResult::none;
    }

    HitResult processGamepadInvocation(
        const ListenerInvocation& invocation,
        ScriptedDrawable* alreadyDispatched) override
    {
        auto hitResult = HitResult::none;
        auto nestedArtboard = m_component->as<NestedArtboard>();
        for (auto nestedAnimation : nestedArtboard->nestedAnimations())
        {
            if (nestedAnimation->is<NestedStateMachine>())
            {
                auto nestedStateMachine =
                    nestedAnimation->as<NestedStateMachine>();
                nestedStateMachine->stateMachineInstance()
                    ->broadcastGamepadToScriptedDrawables(invocation,
                                                          alreadyDispatched);
            }
        }
        return hitResult;
    }
    HitResult processEvent(Vec2D position,
                           ListenerType hitType,
                           bool canHit,
                           float timeStamp,
                           int pointerId,
                           PointerButton button) override
    {
        auto nestedArtboard = m_component->as<NestedArtboard>();
        HitResult hitResult = HitResult::none;
        if (nestedArtboard->isCollapsed() || nestedArtboard->isHidden() ||
            nestedArtboard->isPaused())
        {
            return hitResult;
        }
        Vec2D nestedPosition;
        if (!nestedArtboard->worldToLocal(position, &nestedPosition))
        {
            // Mounted artboard isn't ready or has a 0 scale transform.
            return hitResult;
        }

        for (auto nestedAnimation : nestedArtboard->nestedAnimations())
        {
            if (nestedAnimation->is<NestedStateMachine>())
            {
                auto nestedStateMachine =
                    nestedAnimation->as<NestedStateMachine>();
                if (canHit)
                {
                    switch (hitType)
                    {
                        case ListenerType::down:
                            hitResult =
                                nestedStateMachine->pointerDown(nestedPosition,
                                                                pointerId,
                                                                button);
                            break;
                        case ListenerType::up:
                            hitResult =
                                nestedStateMachine->pointerUp(nestedPosition,
                                                              pointerId,
                                                              button);
                            break;
                        case ListenerType::move:
                            hitResult =
                                nestedStateMachine->pointerMove(nestedPosition,
                                                                timeStamp,
                                                                pointerId);
                            break;
                        case ListenerType::dragStart:
                            nestedStateMachine->dragStart(nestedPosition,
                                                          timeStamp,
                                                          pointerId,
                                                          button);
                            break;
                        case ListenerType::dragEnd:
                            nestedStateMachine->dragEnd(nestedPosition,
                                                        timeStamp,
                                                        pointerId,
                                                        button);
                            break;
                        case ListenerType::exit:
                            hitResult =
                                nestedStateMachine->pointerExit(nestedPosition,
                                                                pointerId);
                            break;
                        case ListenerType::enter:
                        case ListenerType::event:
                        case ListenerType::click:
                        case ListenerType::componentProvided:
                        case ListenerType::textInput:
                        case ListenerType::viewModel:
                        case ListenerType::drag:
                        case ListenerType::focus:
                        case ListenerType::blur:
                        case ListenerType::keyboard:
                        case ListenerType::semanticAction:
                        case ListenerType::gamepad:
                            break;
                    }
                }
                else
                {
                    switch (hitType)
                    {
                        case ListenerType::down:
                        case ListenerType::up:
                        case ListenerType::move:
                        case ListenerType::exit:
                            nestedStateMachine->pointerExit(nestedPosition,
                                                            pointerId);
                            break;
                        case ListenerType::dragStart:
                        case ListenerType::dragEnd:
                        case ListenerType::enter:
                        case ListenerType::event:
                        case ListenerType::click:
                        case ListenerType::componentProvided:
                        case ListenerType::textInput:
                        case ListenerType::viewModel:
                        case ListenerType::drag:
                        case ListenerType::focus:
                        case ListenerType::blur:
                        case ListenerType::keyboard:
                        case ListenerType::semanticAction:
                        case ListenerType::gamepad:
                            break;
                    }
                }
            }
        }
        return hitResult;
    }
    void prepareEvent(Vec2D position,
                      ListenerType hitType,
                      int pointerId) override
    {}
};

class HitComponentList : public HitComponent
{
public:
    HitComponentList(Component* componentList,
                     StateMachineInstance* stateMachineInstance) :
        HitComponent(componentList, stateMachineInstance)
    {}
    ~HitComponentList() override {}

    bool hitTest(Vec2D position) const override
    {
        auto componentList = m_component->as<ArtboardComponentList>();
        if (componentList->isCollapsed() || componentList->isHidden())
        {
            return false;
        }
        const auto& order = componentList->orderedListIndices();
        for (auto it = order.rbegin(); it != order.rend(); ++it)
        {
            const int i = *it;
            Vec2D listPosition;
            if (!componentList->worldToLocal(position, &listPosition, i))
            {
                // Mounted artboard isn't ready or has a 0 scale transform.
                continue;
            }
            auto stateMachine = componentList->stateMachineInstance(i);
            if (stateMachine != nullptr && stateMachine->hitTest(listPosition))
            {
                return true;
            }
        }
        return false;
    }
    bool hitTestBounded(Vec2D position) const override
    {
        auto componentList = m_component->as<ArtboardComponentList>();
        if (componentList->isCollapsed() || componentList->isHidden())
        {
            return false;
        }
        const auto& order = componentList->orderedListIndices();
        for (auto it = order.rbegin(); it != order.rend(); ++it)
        {
            const int i = *it;
            Vec2D listPosition;
            if (!componentList->worldToLocal(position, &listPosition, i))
            {
                continue;
            }
            auto stateMachine = componentList->stateMachineInstance(i);
            if (stateMachine != nullptr &&
                stateMachine->hitTestBounded(listPosition))
            {
                return true;
            }
        }
        return false;
    }

    /// Maps a parent-space delta into item i. A delta is a vector, so it
    /// takes the mapping's linear part only.
    bool itemDelta(ArtboardComponentList* componentList,
                   int i,
                   Vec2D position,
                   Vec2D listPosition,
                   Vec2D delta,
                   Vec2D& out)
    {
        Vec2D shifted;
        if (!componentList->worldToLocal(position + delta, &shifted, i))
        {
            return false;
        }
        out = shifted - listPosition;
        return true;
    }

    // Items walk top-most first, like processEvent, and an opaque item blocks
    // the items behind it the way hitOpaque clears runningCanHit there.
    bool wantsScroll(Vec2D position, const ScrollEvent& event) override
    {
        auto componentList = m_component->as<ArtboardComponentList>();
        if (componentList->isCollapsed())
        {
            return false;
        }
        const auto& order = componentList->orderedListIndices();
        for (auto it = order.rbegin(); it != order.rend(); ++it)
        {
            const int i = *it;
            Vec2D listPosition;
            if (!componentList->worldToLocal(position, &listPosition, i))
            {
                continue;
            }
            auto stateMachine = componentList->stateMachineInstance(i);
            if (stateMachine == nullptr)
            {
                continue;
            }
            ScrollEvent mapped = event;
            if (!itemDelta(componentList,
                           i,
                           position,
                           listPosition,
                           event.delta,
                           mapped.delta))
            {
                continue;
            }
            if (stateMachine->wantsScroll(listPosition, mapped))
            {
                return true;
            }
            if (stateMachine->scrollOccludedAt(listPosition))
            {
                return false;
            }
        }
        return false;
    }

    bool hasScrollTarget(Vec2D position) override
    {
        auto componentList = m_component->as<ArtboardComponentList>();
        if (componentList->isCollapsed())
        {
            return false;
        }
        const auto& order = componentList->orderedListIndices();
        for (auto it = order.rbegin(); it != order.rend(); ++it)
        {
            const int i = *it;
            Vec2D listPosition;
            if (!componentList->worldToLocal(position, &listPosition, i))
            {
                continue;
            }
            auto stateMachine = componentList->stateMachineInstance(i);
            if (stateMachine == nullptr)
            {
                continue;
            }
            if (stateMachine->hasScrollTargetAt(listPosition))
            {
                return true;
            }
            if (stateMachine->scrollOccludedAt(listPosition))
            {
                return false;
            }
        }
        return false;
    }

    bool listensToButtonAt(Vec2D position, PointerButton button) override
    {
        auto componentList = m_component->as<ArtboardComponentList>();
        if (componentList->isCollapsed() || componentList->isHidden())
        {
            return false;
        }
        const auto& order = componentList->orderedListIndices();
        for (auto it = order.rbegin(); it != order.rend(); ++it)
        {
            const int i = *it;
            Vec2D listPosition;
            if (!componentList->worldToLocal(position, &listPosition, i))
            {
                continue;
            }
            auto stateMachine = componentList->stateMachineInstance(i);
            if (stateMachine != nullptr &&
                stateMachine->listensToButtonAt(listPosition, button))
            {
                return true;
            }
        }
        return false;
    }

    bool occludesScroll(Vec2D position) override
    {
        auto componentList = m_component->as<ArtboardComponentList>();
        if (componentList->isCollapsed())
        {
            return false;
        }
        const auto& order = componentList->orderedListIndices();
        for (auto it = order.rbegin(); it != order.rend(); ++it)
        {
            const int i = *it;
            Vec2D listPosition;
            if (!componentList->worldToLocal(position, &listPosition, i))
            {
                continue;
            }
            auto stateMachine = componentList->stateMachineInstance(i);
            if (stateMachine != nullptr &&
                stateMachine->scrollOccludedAt(listPosition))
            {
                return true;
            }
        }
        return false;
    }

    bool scrollGestureActive() override
    {
        auto componentList = m_component->as<ArtboardComponentList>();
        const auto& order = componentList->orderedListIndices();
        if (componentList->isCollapsed())
        {
            // A collapsed list never advances its items, so an idle timer
            // there can never clear the latch. End it outright.
            for (int i : order)
            {
                auto stateMachine = componentList->stateMachineInstance(i);
                if (stateMachine != nullptr)
                {
                    stateMachine->cancelScroll();
                }
            }
            return false;
        }
        for (int i : order)
        {
            auto stateMachine = componentList->stateMachineInstance(i);
            if (stateMachine != nullptr && stateMachine->hasScrollLatch())
            {
                return true;
            }
        }
        return false;
    }

    void cancelScroll() override
    {
        auto componentList = m_component->as<ArtboardComponentList>();
        for (int i : componentList->orderedListIndices())
        {
            auto stateMachine = componentList->stateMachineInstance(i);
            if (stateMachine != nullptr)
            {
                stateMachine->cancelScroll();
            }
        }
    }

    HitResult processScroll(Vec2D position,
                            const ScrollEvent& event,
                            float timeStamp) override
    {
        auto componentList = m_component->as<ArtboardComponentList>();
        if (componentList->isCollapsed())
        {
            return HitResult::none;
        }
        const auto& order = componentList->orderedListIndices();
        // A latched item gets the event first; only then can another take it.
        for (int pass = 0; pass < 2; pass++)
        {
            for (auto it = order.rbegin(); it != order.rend(); ++it)
            {
                const int i = *it;
                auto stateMachine = componentList->stateMachineInstance(i);
                if (stateMachine == nullptr ||
                    (pass == 0 && !stateMachine->hasScrollLatch()))
                {
                    continue;
                }
                Vec2D listPosition;
                if (!componentList->worldToLocal(position, &listPosition, i))
                {
                    continue;
                }
                ScrollEvent mapped = event;
                if (!itemDelta(componentList,
                               i,
                               position,
                               listPosition,
                               event.delta,
                               mapped.delta))
                {
                    continue;
                }
                auto result = stateMachine->pointerScroll(listPosition,
                                                          mapped,
                                                          timeStamp);
                if (pass == 0 || result != HitResult::none)
                {
                    return result;
                }
                if (stateMachine->scrollOccludedAt(listPosition))
                {
                    return HitResult::none;
                }
            }
        }
        return HitResult::none;
    }

    HitResult processEvent(Vec2D position,
                           ListenerType hitType,
                           bool canHit,
                           float timeStamp,
                           int pointerId,
                           PointerButton button) override
    {
        auto componentList = m_component->as<ArtboardComponentList>();
        HitResult hitResult = HitResult::none;
        bool runningCanHit = canHit;
        if (componentList->isCollapsed() || componentList->isHidden())
        {
            return hitResult;
        }
        const auto& order = componentList->orderedListIndices();
        for (auto it = order.rbegin(); it != order.rend(); ++it)
        {
            const int i = *it;
            Vec2D listPosition;
            bool hit = componentList->worldToLocal(position, &listPosition, i);
            if (!hit)
            {
                continue;
            }
            auto stateMachine = componentList->stateMachineInstance(i);
            if (stateMachine != nullptr)
            {
                HitResult itemHitResult = HitResult::none;
                if (runningCanHit)
                {
                    switch (hitType)
                    {
                        case ListenerType::down:
                            itemHitResult =
                                stateMachine->pointerDown(listPosition,
                                                          pointerId,
                                                          button);
                            break;
                        case ListenerType::up:
                            itemHitResult =
                                stateMachine->pointerUp(listPosition,
                                                        pointerId,
                                                        button);
                            break;
                        case ListenerType::move:
                            itemHitResult =
                                stateMachine->pointerMove(listPosition,
                                                          timeStamp,
                                                          pointerId);
                            break;
                        case ListenerType::exit:
                            itemHitResult =
                                stateMachine->pointerExit(listPosition,
                                                          pointerId);
                            break;
                        case ListenerType::dragStart:
                            stateMachine->dragStart(listPosition,
                                                    0,
                                                    true,
                                                    pointerId,
                                                    button);
                            break;
                        case ListenerType::dragEnd:
                            stateMachine->dragEnd(listPosition,
                                                  0,
                                                  pointerId,
                                                  button);
                            break;
                        case ListenerType::enter:
                        case ListenerType::event:
                        case ListenerType::click:
                        case ListenerType::componentProvided:
                        case ListenerType::textInput:
                        case ListenerType::viewModel:
                        case ListenerType::drag:
                        case ListenerType::focus:
                        case ListenerType::blur:
                        case ListenerType::keyboard:
                        case ListenerType::semanticAction:
                        case ListenerType::gamepad:
                            break;
                    }
                }
                else
                {
                    switch (hitType)
                    {
                        case ListenerType::down:
                        case ListenerType::up:
                        case ListenerType::move:
                        case ListenerType::exit:
                            stateMachine->pointerExit(listPosition, pointerId);
                            break;
                        case ListenerType::dragStart:
                        case ListenerType::dragEnd:
                        case ListenerType::enter:
                        case ListenerType::event:
                        case ListenerType::click:
                        case ListenerType::componentProvided:
                        case ListenerType::textInput:
                        case ListenerType::viewModel:
                        case ListenerType::drag:
                        case ListenerType::focus:
                        case ListenerType::blur:
                        case ListenerType::keyboard:
                        case ListenerType::semanticAction:
                        case ListenerType::gamepad:
                            break;
                    }
                }
                if ((hitResult == HitResult::none &&
                     (itemHitResult == HitResult::hit ||
                      itemHitResult == HitResult::hitOpaque)) ||
                    (hitResult == HitResult::hit &&
                     itemHitResult == HitResult::hitOpaque))
                {
                    hitResult = itemHitResult;
                }
                if (hitResult == HitResult::hitOpaque)
                {
                    runningCanHit = false;
                }
            }
        }
        return hitResult;
    }
    HitResult processGamepadInvocation(
        const ListenerInvocation& invocation,
        ScriptedDrawable* alreadyDispatched) override
    {
        auto componentList = m_component->as<ArtboardComponentList>();
        HitResult hitResult = HitResult::none;
        bool runningCanHit = true;
        if (componentList->isCollapsed())
        {
            return hitResult;
        }
        const auto& order = componentList->orderedListIndices();
        for (auto it = order.rbegin(); it != order.rend(); ++it)
        {
            const int i = *it;
            auto stateMachine = componentList->stateMachineInstance(i);
            if (stateMachine != nullptr)
            {
                HitResult itemHitResult = HitResult::none;
                if (runningCanHit)
                {
                    itemHitResult =
                        stateMachine->broadcastGamepadToScriptedDrawables(
                            invocation,
                            alreadyDispatched);
                }

                if ((hitResult == HitResult::none &&
                     (itemHitResult == HitResult::hit ||
                      itemHitResult == HitResult::hitOpaque)) ||
                    (hitResult == HitResult::hit &&
                     itemHitResult == HitResult::hitOpaque))
                {
                    hitResult = itemHitResult;
                }
                if (hitResult == HitResult::hitOpaque)
                {
                    runningCanHit = false;
                }
            }
        }
        return hitResult;
    }
    void prepareEvent(Vec2D position,
                      ListenerType hitType,
                      int pointerId) override
    {}
};

class ListenerViewModel;

// Helper that holds one view model property reference, listens to its dirt,
// and reports the parent ListenerViewModel when the property changes.
class ListenerViewModelPropertyBinding : public ViewModelValueDependent
{
public:
    ListenerViewModelPropertyBinding(ListenerViewModel* parent,
                                     ViewModelInstanceValue* vmProp);
    virtual ~ListenerViewModelPropertyBinding();
    void addDirt(ComponentDirt value, bool recurse) override;
    void relinkDataBind() override;
    ViewModelInstanceValue* value() { return m_viewModelInstanceValue.get(); }

protected:
    ListenerViewModel* m_parent = nullptr;
    rive::rcp<ViewModelInstanceValue> m_viewModelInstanceValue = nullptr;
    void clearDataContext();
};
class ListenerViewModelPropertyBindingListener
    : public ListenerViewModelPropertyBinding
{
public:
    ListenerViewModelPropertyBindingListener(
        ListenerViewModel* parent,
        ViewModelInstanceValue* vmProp,
        const StateMachineListenerSingle* listener);
    void relinkDataBind() override;

private:
    const StateMachineListenerSingle* m_listener;
};
class ListenerViewModelPropertyBindingInput
    : public ListenerViewModelPropertyBinding
{
public:
    ListenerViewModelPropertyBindingInput(
        ListenerViewModel* parent,
        ViewModelInstanceValue* vmProp,
        const ListenerInputTypeViewModel* listenerInput);
    void relinkDataBind() override;

private:
    const ListenerInputTypeViewModel* m_listenerInput;
};

class ListenerViewModel
{
public:
    virtual ~ListenerViewModel();
    ListenerViewModel(StateMachineInstance* smInstance,
                      const StateMachineListener* listener) :
        m_stateMachineInstance(smInstance), m_listener(listener)
    {}

    void clearDataContext() { m_propertyBindings.clear(); }
    void bindFromContext(rcp<DataContext> dataContext)
    {
        m_dataContext = dataContext;
        clearDataContext();
        if (m_listener->is<StateMachineListenerSingle>())
        {
            auto vmProp = dataContext->getViewModelProperty(
                m_listener->as<StateMachineListenerSingle>()->dataBindPath());
            if (vmProp != nullptr)
            {
                m_propertyBindings.push_back(
                    std::make_unique<ListenerViewModelPropertyBindingListener>(
                        this,
                        vmProp,
                        m_listener->as<StateMachineListenerSingle>()));
            }
        }
        else
        {
            size_t index = 0;
            while (index < m_listener->listenerInputTypeCount())
            {
                auto listenerInputType = m_listener->listenerInputType(index);
                if (listenerInputType->is<ListenerInputTypeViewModel>())
                {
                    auto listenerInputTypeVM =
                        listenerInputType->as<ListenerInputTypeViewModel>();
                    auto vmProp = dataContext->getViewModelProperty(
                        listenerInputTypeVM->dataBindPath());
                    if (vmProp != nullptr)
                    {
                        m_propertyBindings.push_back(
                            std::make_unique<
                                ListenerViewModelPropertyBindingInput>(
                                this,
                                vmProp,
                                listenerInputTypeVM));
                    }
                }
                index++;
            }
        }
        // A trigger fired before this bind (e.g. during script init) stays
        // pending until the frame resets it; report it so it isn't lost.
        for (auto& binding : m_propertyBindings)
        {
            auto value = binding->value();
            if (value != nullptr && value->is<ViewModelInstanceTrigger>() &&
                value->as<ViewModelInstanceTrigger>()->propertyValue() != 0)
            {
                reportToStateMachine(value);
            }
        }
    }
    void reportToStateMachine(ViewModelInstanceValue* value)
    {
        if (!value->is<ViewModelInstanceTrigger>() ||
            value->as<ViewModelInstanceTrigger>()->propertyValue() != 0)
        {
            m_stateMachineInstance->reportListenerViewModel(this);
        }
    }
    const StateMachineListener* listener() { return m_listener; }
    DataContext* dataContext()
    {
        if (m_dataContext)
        {

            return m_dataContext.get();
        }
        return nullptr;
    }

private:
    StateMachineInstance* m_stateMachineInstance = nullptr;
    const StateMachineListener* m_listener = nullptr;
    rcp<DataContext> m_dataContext = nullptr;
    std::vector<std::unique_ptr<ListenerViewModelPropertyBinding>>
        m_propertyBindings;
};

ListenerViewModelPropertyBinding::ListenerViewModelPropertyBinding(
    ListenerViewModel* parent,
    ViewModelInstanceValue* vmProp) :
    m_parent(parent), m_viewModelInstanceValue(rive::ref_rcp(vmProp))
{
    vmProp->addDependent(this);
}

void ListenerViewModelPropertyBinding::relinkDataBind() {}

ListenerViewModelPropertyBinding::~ListenerViewModelPropertyBinding()
{
    clearDataContext();
}

void ListenerViewModelPropertyBinding::clearDataContext()
{

    if (m_viewModelInstanceValue != nullptr)
    {
        m_viewModelInstanceValue->removeDependent(this);
        m_viewModelInstanceValue = nullptr;
    }
}

ListenerViewModelPropertyBindingListener::
    ListenerViewModelPropertyBindingListener(
        ListenerViewModel* parent,
        ViewModelInstanceValue* vmProp,
        const StateMachineListenerSingle* listener) :
    ListenerViewModelPropertyBinding(parent, vmProp), m_listener(listener)
{}

void ListenerViewModelPropertyBindingListener::relinkDataBind()
{
    auto dataContext = m_parent->dataContext();
    if (dataContext)
    {

        auto vmProp =
            dataContext->getViewModelProperty(m_listener->dataBindPath());
        if (vmProp != m_viewModelInstanceValue.get())
        {
            clearDataContext();
            if (vmProp != nullptr)
            {
                m_viewModelInstanceValue = ref_rcp(vmProp);
                vmProp->addDependent(this);
            }
        }
    }
}

ListenerViewModelPropertyBindingInput::ListenerViewModelPropertyBindingInput(
    ListenerViewModel* parent,
    ViewModelInstanceValue* vmProp,
    const ListenerInputTypeViewModel* listenerInput) :
    ListenerViewModelPropertyBinding(parent, vmProp),
    m_listenerInput(listenerInput)
{}

void ListenerViewModelPropertyBindingInput::relinkDataBind()
{
    auto dataContext = m_parent->dataContext();
    if (dataContext)
    {
        auto vmProp =
            dataContext->getViewModelProperty(m_listenerInput->dataBindPath());
        if (vmProp != m_viewModelInstanceValue.get())
        {
            clearDataContext();
            if (vmProp != nullptr)
            {
                m_viewModelInstanceValue = ref_rcp(vmProp);
                vmProp->addDependent(this);
            }
        }
    }
}

void ListenerViewModelPropertyBinding::addDirt(ComponentDirt value,
                                               bool recurse)
{
    if (m_parent != nullptr && m_viewModelInstanceValue != nullptr)
    {
        m_parent->reportToStateMachine(m_viewModelInstanceValue.get());
    }
}

ListenerViewModel::~ListenerViewModel() { clearDataContext(); }

} // namespace rive

HitResult StateMachineInstance::updateListeners(Vec2D position,
                                                ListenerType hitType,
                                                int pointerId,
                                                float timeStamp,
                                                PointerButton button)
{
    // Listener hit-testing runs in content space, so undo the frame origin and
    // the artboard's own rotation/scale first.
    //
    // A degenerate (0 scale) self transform has no inverse: the contents
    // collapse to nothing, so nothing in them can be hit. We still run the pass
    // with every group forced to miss rather than returning early, so hover
    // unwinds and pending exits fire, and we cancel any gesture in flight so a
    // press held across the collapse can't resume when the scale comes back.
    bool contentsCollapsed = !mapToContentSpace(position);

    // First reset all listener groups before processing the events
    for (const auto& listenerGroup : m_listenerGroups)
    {
        listenerGroup.get()->reset(pointerId);
    }
    // Drag ends owed by cancellation, dispatched once the pass below has had a
    // chance to emit its hover exits.
    std::vector<std::pair<int, PointerButton>> dragEnded;
    if (contentsCollapsed)
    {
        // canHit alone won't do this: it marks a target as occluded, and an
        // occluded target deliberately keeps its press so a drag survives the
        // pointer moving over other things (see ListenerGroup::processEvent,
        // where the phase only resets on down/up and the drag branch ignores
        // canHit). Collapsed content isn't occluded, it's gone.
        //
        // Every tracked pointer is cancelled, not just the one that delivered
        // this event: the contents are gone for all of them. The drag ends are
        // only collected here -- dispatching one re-enters updateListeners,
        // whose reset() overwrites isPrevHovered with the isHovered this pass
        // already cleared, so any exit still pending would be swallowed, and
        // whose enablePointerEvents() resets every group's phase, so groups not
        // yet cancelled would look like they had nothing in flight.
        for (const auto& listenerGroup : m_listenerGroups)
        {
            listenerGroup.get()->cancelPointers(position, timeStamp, dragEnded);
        }
    }
    else
    {
        // Next prepare the event to set the common hover status for each group.
        // Skipped when collapsed so every group stays unhovered.
        for (const auto& hitShape : m_hitComponents)
        {
            hitShape->prepareEvent(position, hitType, pointerId);
        }
    }
    bool hitSomething = false;
    bool hitOpaque = false;
    // Process the events
    for (const auto& hitShape : m_hitComponents)
    {
        HitResult hitResult =
            hitShape->processEvent(position,
                                   hitType,
                                   !hitOpaque && !contentsCollapsed,
                                   timeStamp,
                                   pointerId,
                                   button);
        if (hitResult != HitResult::none)
        {
            hitSomething = true;
            if (hitResult == HitResult::hitOpaque)
            {
                hitOpaque = true;
            }
        }
    }
    // Hover exits have been emitted, so it's safe to let dragEnd re-enter now.
    for (auto& ended : dragEnded)
    {
        dragEnd(position, timeStamp, ended.first, ended.second);
    }
    // Finally release events that are complete
    if (hitType == ListenerType::exit)
    {
        for (const auto& listenerGroup : m_listenerGroups)
        {
            listenerGroup.get()->releaseEvent(pointerId);
        }
    }

    return hitSomething ? hitOpaque ? HitResult::hitOpaque : HitResult::hit
                        : HitResult::none;
}

bool StateMachineInstance::hitTest(Vec2D position) const
{
    return hitTestInternal(position, false);
}

bool StateMachineInstance::hitTestBounded(Vec2D position) const
{
    return hitTestInternal(position, true);
}

bool StateMachineInstance::hitTestInternal(Vec2D position, bool bounded) const
{
    if (m_artboardInstance->frameOrigin())
    {
        position -= Vec2D(
            m_artboardInstance->originX() * m_artboardInstance->layoutWidth(),
            m_artboardInstance->originY() * m_artboardInstance->layoutHeight());
    }
    // Invert the artboard's own rotation/scale (applied in drawInternal after
    // the frame-origin translation) so the pointer maps into content space.
    // Covers nested state machines too, which funnel through here.
    if (m_artboardInstance->hasSelfTransform())
    {
        Mat2D inverse;
        if (!m_artboardInstance->selfTransform().invert(&inverse))
        {
            // A degenerate (0 scale) self transform collapses the contents to
            // nothing, so there's nothing to hit.
            return false;
        }
        position = inverse * position;
    }

    for (const auto& hitShape : m_hitComponents)
    {
        // TODO: quick reject.

        if (bounded ? hitShape->hitTestBounded(position)
                    : hitShape->hitTest(position))
        {
            return true;
        }
    }
    return false;
}

HitResult StateMachineInstance::pointerMove(Vec2D position,
                                            float timeStamp,
                                            int id)
{
    return updateListeners(position, ListenerType::move, id, timeStamp);
}
HitResult StateMachineInstance::pointerDown(Vec2D position,
                                            int id,
                                            PointerButton button)
{
    return updateListeners(position, ListenerType::down, id, 0, button);
}
HitResult StateMachineInstance::pointerUp(Vec2D position,
                                          int id,
                                          PointerButton button)
{
    return updateListeners(position, ListenerType::up, id, 0, button);
}
HitResult StateMachineInstance::pointerExit(Vec2D position, int id)
{
    return updateListeners(position, ListenerType::exit, id);
}
HitResult StateMachineInstance::dragStart(Vec2D position,
                                          float timeStamp,
                                          bool disablePointer,
                                          int pointerId,
                                          PointerButton button)
{
    if (disablePointer)
    {
        disablePointerEvents(pointerId);
    }
    auto hit = updateListeners(position,
                               ListenerType::dragStart,
                               pointerId,
                               0,
                               button);
    return hit;
}
HitResult StateMachineInstance::dragEnd(Vec2D position,
                                        float timeStamp,
                                        int pointerId,
                                        PointerButton button)
{
    enablePointerEvents(pointerId);
    auto hit =
        updateListeners(position, ListenerType::dragEnd, pointerId, 0, button);
    pointerMove(position, timeStamp, pointerId);
    return hit;
}
bool StateMachineInstance::mapToContentSpace(Vec2D& position,
                                             Vec2D* delta) const
{
    if (m_artboardInstance->frameOrigin())
    {
        position -= Vec2D(
            m_artboardInstance->originX() * m_artboardInstance->layoutWidth(),
            m_artboardInstance->originY() * m_artboardInstance->layoutHeight());
    }
    if (m_artboardInstance->hasSelfTransform())
    {
        Mat2D inverse;
        if (!m_artboardInstance->selfTransform().invert(&inverse))
        {
            return false;
        }
        position = inverse * position;
        if (delta != nullptr)
        {
            // A delta is a vector: linear part only, no translation.
            *delta = inverse * *delta - inverse * Vec2D();
        }
    }
    return true;
}

HitResult StateMachineInstance::pointerScroll(Vec2D position,
                                              const ScrollEvent& event,
                                              float timeStamp,
                                              int pointerId)
{
    ScrollEvent mapped = event;
    if (!mapToContentSpace(position, &mapped.delta))
    {
        // Nothing can map through a singular transform, so no later event
        // could close this gesture.
        cancelScroll();
        return HitResult::none;
    }
    if (m_scrollLatch != nullptr && !m_scrollLatch->scrollGestureActive())
    {
        m_scrollLatch = nullptr;
    }
    HitComponent* target = m_scrollLatch;
    if (target == nullptr)
    {
        // Sorted top-most first, so the innermost view that can move wins.
        // An opaque hit that cannot scroll ends the walk, as it would for a
        // pointer: nothing behind it may take the event.
        for (const auto& hitShape : m_hitComponents)
        {
            if (hitShape->wantsScroll(position, mapped))
            {
                target = hitShape.get();
                break;
            }
            if (hitShape->occludesScroll(position))
            {
                break;
            }
        }
    }
    if (target == nullptr)
    {
        return HitResult::none;
    }
    auto result = target->processScroll(position, mapped, timeStamp);
    m_scrollLatch = target->scrollGestureActive() ? target : nullptr;
    return result;
}

bool StateMachineInstance::wantsScroll(Vec2D position, const ScrollEvent& event)
{
    if (hasScrollLatch())
    {
        return true;
    }
    ScrollEvent mapped = event;
    if (!mapToContentSpace(position, &mapped.delta))
    {
        return false;
    }
    for (const auto& hitShape : m_hitComponents)
    {
        if (hitShape->wantsScroll(position, mapped))
        {
            return true;
        }
        if (hitShape->occludesScroll(position))
        {
            return false;
        }
    }
    return false;
}

bool StateMachineInstance::hasScrollTargetAt(Vec2D position)
{
    if (hasScrollLatch())
    {
        return true;
    }
    if (!mapToContentSpace(position))
    {
        return false;
    }
    for (const auto& hitShape : m_hitComponents)
    {
        if (hitShape->hasScrollTarget(position))
        {
            return true;
        }
        if (hitShape->occludesScroll(position))
        {
            return false;
        }
    }
    return false;
}

bool StateMachineInstance::listensToButtonAt(Vec2D position,
                                             PointerButton button)
{
    if (!mapToContentSpace(position))
    {
        return false;
    }
    for (const auto& hitShape : m_hitComponents)
    {
        if (hitShape->listensToButtonAt(position, button))
        {
            return true;
        }
        if (hitShape->occludesPointer(position))
        {
            return false;
        }
    }
    return false;
}

bool StateMachineInstance::scrollOccludedAt(Vec2D position)
{
    if (!mapToContentSpace(position))
    {
        // Collapsed contents hit nothing, so they hide nothing either.
        return false;
    }
    // Only asked once nothing here wanted the event, so the first opaque hit
    // decides.
    for (const auto& hitShape : m_hitComponents)
    {
        if (hitShape->occludesScroll(position))
        {
            return true;
        }
    }
    return false;
}

void StateMachineInstance::cancelScroll()
{
    if (m_scrollLatch != nullptr)
    {
        m_scrollLatch->cancelScroll();
        m_scrollLatch = nullptr;
    }
}

bool StateMachineInstance::hasScrollLatch()
{
    if (m_scrollLatch != nullptr && !m_scrollLatch->scrollGestureActive())
    {
        m_scrollLatch = nullptr;
    }
    return m_scrollLatch != nullptr;
}

#ifdef TESTING
uint64_t StateMachineInstance::sm_settledLayerSkips = 0;
uint64_t StateMachineInstance::sm_frozenLayerAdvances = 0;

const LayerState* StateMachineInstance::layerState(size_t index)
{
    if (index < m_machine->layerCount())
    {
        return m_layers[index].currentState();
    }
    return nullptr;
}
#endif

void StateMachineInstance::addToHitLookup(
    Component* target,
    bool isLayoutComponent,
    std::unordered_map<Component*, HitDrawable*>& hitLookup,
    ListenerGroup* listenerGroup,
    bool isOpaque)
{
    // target could either be a LayoutComponent or a DrawableProxy
    if (isLayoutComponent)
    {
        HitLayout* hitLayout;
        auto itr = hitLookup.find(target);
        if (itr == hitLookup.end())
        {
            auto hs = std::make_unique<HitLayout>(target->as<Drawable>(),
                                                  this,
                                                  isOpaque);
            hitLookup[target] = hitLayout = hs.get();
            m_hitComponents.push_back(std::move(hs));
        }
        else
        {
            hitLayout = static_cast<HitLayout*>(itr->second);
        }
        hitLayout->addListener(listenerGroup);
        if (isOpaque)
        {
            hitLayout->isOpaque = true;
        }
        return;
    }

    if (target->is<Shape>())
    {
        HitExpandable* hitShape;
        auto itr = hitLookup.find(target);
        if (itr == hitLookup.end())
        {
            Shape* shape = target->as<Shape>();
            shape->addFlags(PathFlags::neverDeferUpdate);
            shape->addDirt(ComponentDirt::Path, true);
            auto hs = std::make_unique<HitExpandable>(shape, shape, this);
            hitLookup[target] = hitShape = hs.get();
            m_hitComponents.push_back(std::move(hs));
        }
        else
        {
            hitShape = static_cast<HitExpandable*>(itr->second);
        }
        hitShape->addListener(listenerGroup);
        return;
    }

    if (target->is<TextValueRun>())
    {
        HitTextRun* hitTextRun;
        auto itr = hitLookup.find(target);
        if (itr == hitLookup.end())
        {
            TextValueRun* run = target->as<TextValueRun>();
            run->textComponent()->addDirt(ComponentDirt::Path, true);
            auto hs =
                std::make_unique<HitTextRun>(run->textComponent(), run, this);
            hitLookup[target] = hitTextRun = hs.get();
            m_hitComponents.push_back(std::move(hs));
        }
        else
        {
            hitTextRun = static_cast<HitTextRun*>(itr->second);
        }
        hitTextRun->addListener(listenerGroup);
        return;
    }

    if (target->is<ContainerComponent>())
    {
        target->as<ContainerComponent>()->forEachChild([&](Component* child) {
            addToHitLookup(child,
                           child->is<LayoutComponent>(),
                           hitLookup,
                           listenerGroup,
                           isOpaque);
            return false;
        });
        return;
    }
}

StateMachineInstance::StateMachineInstance(const StateMachine* machine,
                                           ArtboardInstance* instance) :
    Scene(instance),
    m_machine(machine),
    m_bindsPending(false),
    m_anySettled(false),
    m_destroying(false)
{
    const auto count = machine->inputCount();
    m_inputInstances.resize(count);
    for (size_t i = 0; i < count; i++)
    {
        auto input = machine->input(i);
        if (input == nullptr)
        {
            continue;
        }
        switch (input->coreType())
        {
            case StateMachineBool::typeKey:
                m_inputInstances[i] =
                    new SMIBool(input->as<StateMachineBool>(), this);
                break;
            case StateMachineNumber::typeKey:
                m_inputInstances[i] =
                    new SMINumber(input->as<StateMachineNumber>(), this);
                break;
            case StateMachineTrigger::typeKey:
                m_inputInstances[i] =
                    new SMITrigger(input->as<StateMachineTrigger>(), this);
                break;
            default:
                // Sanity check.
                break;
        }
#ifdef WITH_RIVE_TOOLS
        auto instance = m_inputInstances[i];
        if (instance != nullptr)
        {
            instance->m_index = i;
        }
#endif
    }

    // Seeded once per state machine instance. This used to run inside the
    // per-layer init(), reseeding the global RNG (and, outside deterministic
    // mode, reading the clock) once for every layer of every instance.
    if (File::deterministicMode)
    {
        srand((unsigned int)1);
    }
    else
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         now.time_since_epoch())
                         .count();
        srand((unsigned int)nanos);
    }

    m_layerCount = static_cast<uint32_t>(machine->layerCount());
    m_layers = new StateMachineLayerInstance[m_layerCount];
    for (size_t i = 0; i < m_layerCount; i++)
    {
        m_layers[i].init(this, machine->layer(i));
    }

    // Initialize dataBinds. All databinds are cloned for the state machine
    // instance. That enables binding each instance to its own context without
    // polluting the rest.
    auto dataBindCount = machine->dataBindCount();
    for (size_t i = 0; i < dataBindCount; i++)
    {
        auto dataBind = machine->dataBind(i);
        if (!dataBind->target())
        {
            continue;
        }
        auto dataBindClone = static_cast<DataBind*>(dataBind->clone());
        dataBindClone->file(dataBind->file());
        if (dataBind->converter() != nullptr)
        {
            dataBindClone->converter(
                dataBind->converter()->clone()->as<DataConverter>());
        }
        if (dataBind->target()->is<BindableProperty>())
        {
            auto& bindables = ensureBindables();
            auto bindableProperty = dataBind->target()->as<BindableProperty>();
            BindableProperty* bindablePropertyClone =
                bindables.propertyInstances.find(bindableProperty);
            if (bindablePropertyClone == nullptr)
            {
                bindablePropertyClone =
                    bindableProperty->clone()->as<BindableProperty>();
                bindables.propertyInstances.insert(bindableProperty,
                                                   bindablePropertyClone);
            }
            dataBindClone->target(bindablePropertyClone);
            // We are only storing in this unordered map data binds that are
            // targetting the source. For now, this is only the case for
            // listener actions.
            if ((static_cast<DataBindFlags>(dataBindClone->flags()) &
                 DataBindFlags::ToSource) == DataBindFlags::ToSource)
            {
                bindables.dataBindsToSource[bindablePropertyClone] =
                    dataBindClone;
            }
            else
            {
                bindables.dataBindsToTarget[bindablePropertyClone] =
                    dataBindClone;
            }
        }
        else
        {
            auto* originalTarget = dataBind->target();
            dataBindClone->target(originalTarget);
            if (originalTarget->is<StateTransitionBase>())
            {
                // Create a per-instance BindablePropertyNumber to
                // receive the data-bound value instead of writing
                // to the shared StateTransition. Swap the target
                // and propertyKey so the normal apply() path writes
                // to our instance-local property.
                auto* prop = new BindablePropertyNumber();
                auto& transitionProps =
                    ensureBindables().transitionPropertyInstances;
                transitionProps[originalTarget][dataBind->propertyKey()] = prop;
                dataBindClone->target(prop);
                dataBindClone->propertyKey(
                    BindablePropertyNumberBase::propertyValuePropertyKey);
            }
        }
        // Added once its target is set: addDataBind decides from the target
        // whether a toSource bind hears about changes or has to poll for them,
        // and without one it would poll on every pass.
        addDataBind(dataBindClone);
    }

    // Initialize listeners. Store a lookup table of shape id to hit shape
    // representation (an object that stores all the listeners triggered by the
    // shape producing a listener).
    std::unordered_map<Component*, HitDrawable*> hitLookup;
    for (std::size_t i = 0; i < machine->listenerCount(); i++)
    {
        auto listener = machine->listener(i);
        if (listener->hasListener(ListenerType::event))
        {
            continue;
        }
        if (listener->hasListener(ListenerType::viewModel))
        {
            auto vmListener = new ListenerViewModel(this, listener);
            ensureReporting().listenerViewModels.push_back(vmListener);
            continue;
        }
        // Handle focus/blur listeners - they're driven by FocusManager,
        // not pointer events.
        if (listener->hasListener(ListenerType::focus) ||
            listener->hasListener(ListenerType::blur))
        {
            auto target = m_artboardInstance->resolve(listener->targetId());
            if (target != nullptr && target->is<Node>())
            {
                auto node = target->as<Node>();
                // Find FocusData child of the node
                FocusData* focusData = nullptr;
                for (auto child : node->children())
                {
                    if (child->is<FocusData>())
                    {
                        focusData = child->as<FocusData>();
                        break;
                    }
                }
                if (focusData != nullptr)
                {
                    auto focusGroup =
                        std::make_unique<FocusListenerGroup>(focusData,
                                                             listener,
                                                             this);
                    ensureInputExtras().focusListenerGroups.push_back(
                        std::move(focusGroup));
                }
            }
        }
        if (listener->hasListener(ListenerType::keyboard) ||
            listener->hasListener(ListenerType::textInput))
        {
            auto target = m_artboardInstance->resolve(listener->targetId());
            if (target != nullptr && target->is<Node>())
            {
                auto node = target->as<Node>();
                // Find FocusData child of the node
                FocusData* focusData = nullptr;
                for (auto child : node->children())
                {
                    if (child->is<FocusData>())
                    {
                        focusData = child->as<FocusData>();
                        break;
                    }
                }
                if (focusData != nullptr)
                {
                    auto keyboardGroup =
                        std::make_unique<KeyboardListenerGroup>(focusData,
                                                                listener,
                                                                this);
                    ensureInputExtras().keyboardListenerGroups.push_back(
                        std::move(keyboardGroup));
                }
            }
        }
        // Semantic listeners are driven by accessibility actions rather
        // than pointer events. The editor enforces that the listener's
        // target Node owns a SemanticData child directly; no ancestor
        // walk is performed here.
        if (listener->hasListener(ListenerType::semanticAction))
        {
            auto target = m_artboardInstance->resolve(listener->targetId());
            if (target != nullptr && target->is<Node>())
            {
                for (auto* child : target->as<Node>()->children())
                {
                    if (child->is<SemanticData>())
                    {
                        ensureInputExtras().semanticListenerGroups.push_back(
                            std::make_unique<SemanticListenerGroup>(
                                child->as<SemanticData>(),
                                listener,
                                this));
                        break;
                    }
                }
            }
        }

        if (listener->hasPointerListeners())
        {
            auto listenerGroup = std::make_unique<ListenerGroup>(listener);
            auto target = m_artboardInstance->resolve(listener->targetId());
            if (target != nullptr && target->is<Component>())
            {
                bool isLayoutComponent = false;
                if (target->is<LayoutComponent>())
                {
                    isLayoutComponent = true;
                    target = target->as<LayoutComponent>()->proxy();
                }
                addToHitLookup(target->as<Component>(),
                               isLayoutComponent,
                               hitLookup,
                               listenerGroup.get(),
                               false);
            }
            m_listenerGroups.push_back(std::move(listenerGroup));
        }
        if (listener->hasListener(ListenerType::gamepad))
        {
            auto target = m_artboardInstance->resolve(listener->targetId());
            if (target != nullptr && target->is<Node>())
            {
                auto node = target->as<Node>();
                FocusData* focusData = nullptr;
                for (auto child : node->children())
                {
                    if (child->is<FocusData>())
                    {
                        focusData = child->as<FocusData>();
                        break;
                    }
                }
                if (focusData != nullptr)
                {
                    auto gamepadGroup =
                        std::make_unique<GamepadListenerGroup>(focusData,
                                                               listener,
                                                               this);
                    ensureInputExtras().gamepadListenerGroups.push_back(
                        std::move(gamepadGroup));
                }
            }
        }
    }

    std::vector<ListenerGroupProvider*> componentProvidedListenerGroups;
    for (auto core : m_artboardInstance->objects())
    {
        if (core == nullptr)
        {
            continue;
        }
        auto provider = ListenerGroupProvider::from(core);
        if (provider != nullptr)
        {
            componentProvidedListenerGroups.push_back(provider);
        }
    }
    for (auto component : componentProvidedListenerGroups)
    {
        auto groupsWithTargets = component->listenerGroups();
        for (auto groupWithTargets : groupsWithTargets)
        {
            auto group = groupWithTargets->group();
            auto targets = groupWithTargets->targets();
            for (auto target : targets)
            {
                auto component = target->component();
                bool isLayoutComponent = component->is<LayoutComponent>() ||
                                         (component->is<Drawable>() &&
                                          component->as<Drawable>()->isProxy());
                addToHitLookup(target->component(),
                               isLayoutComponent,
                               hitLookup,
                               group,
                               target->isOpaque());
            }
            m_listenerGroups.push_back(std::unique_ptr<ListenerGroup>(group));
            for (auto target : targets)
            {
                delete target;
            }
            delete groupWithTargets;
        }
        auto hitComponents = component->hitComponents(this);
        for (auto* hitComponent : hitComponents)
        {
            m_hitComponents.push_back(
                std::unique_ptr<HitComponent>(hitComponent));
        }
    }

    for (auto nestedArtboard : instance->nestedArtboards())
    {
        // TODO: @hernan as an optimization only create a HitNestedArtboard if
        // the nested artboard has state machines or if it is bound via data
        // binding
        auto hn =
            std::make_unique<HitNestedArtboard>(nestedArtboard->as<Component>(),
                                                this);
        m_hitComponents.push_back(std::move(hn));
        for (auto animation : nestedArtboard->nestedAnimations())
        {
            if (animation->is<NestedStateMachine>())
            {
                if (auto notifier = animation->as<NestedStateMachine>()
                                        ->stateMachineInstance())
                {
                    notifier->setNestedArtboard(nestedArtboard);
                    notifier->addNestedEventListener(this);
                }
            }
            else if (animation->is<NestedLinearAnimation>())
            {
                if (auto notifier = animation->as<NestedLinearAnimation>()
                                        ->animationInstance())
                {
                    notifier->setNestedArtboard(nestedArtboard);
                    notifier->addNestedEventListener(this);
                }
            }
        }
    }
    for (auto componentList : instance->artboardComponentLists())
    {
        auto hc =
            std::make_unique<HitComponentList>(componentList->as<Component>(),
                                               this);
        m_hitComponents.push_back(std::move(hc));
    }

#ifdef WITH_RIVE_TEXT
    // Register TextInputs as hit targets for drag-to-select functionality
    for (auto textInput : instance->objects<TextInput>())
    {
        auto textInputGroup =
            std::make_unique<TextInputListenerGroup>(textInput, this);
        auto hitExpandable = std::make_unique<HitExpandable>(
            textInput->as<Drawable>(),
            textInput->as<Component>(),
            this,
            true); // isOpaque - TextInput blocks hits behind it
        hitExpandable->addListener(textInputGroup.get());
        m_hitComponents.push_back(std::move(hitExpandable));
        m_listenerGroups.push_back(std::move(textInputGroup));
    }
#endif

    // Initialize local instances of ScriptedObjects, in the state machine's
    // authored order so every downstream walk (dataContext, Lua init) is
    // deterministic.
    auto sharedScriptedObjects = machine->scriptedObjects();
    if (!sharedScriptedObjects.empty())
    {
        auto& scripting = ensureScripting();
        scripting.objects.reserve(sharedScriptedObjects.size());
        for (auto& scriptedOb : sharedScriptedObjects)
        {
            scripting.objects.emplace_back(
                scriptedOb,
                scriptedOb->cloneScriptedObject(this));
        }
        for (auto& scriptedPair : scripting.objects)
        {
            scriptedPair.second->dataContext(m_artboardInstance->dataContext());
        }
        initScriptedObjects();
    }
    // Register Scripted objects as keyboard and text targets when expected,
    // and collect every scripted drawable that wants gamepad events so we can
    // broadcast to it later regardless of focus.
    for (auto object : instance->objects<ContainerComponent>())
    {
        auto scriptedObject = ScriptedObject::from(object);
        if (!scriptedObject)
        {
            continue;
        }
        if (scriptedObject->wantsKeyboardInput() ||
            scriptedObject->wantsTextInput())
        {
            for (auto& child : object->as<ContainerComponent>()->children())
            {
                if (child->is<FocusData>())
                {

                    auto keyboardGroup =
                        std::make_unique<KeyboardListenerGroup>(
                            child->as<FocusData>(),
                            nullptr,
                            this);
                    ensureInputExtras().keyboardListenerGroups.push_back(
                        std::move(keyboardGroup));
                    break;
                }
            }
        }
        if ((scriptedObject->wantsGamePadConnect() ||
             scriptedObject->wantsGamePadDisconnect() ||
             scriptedObject->wantsGamePadEvent()) &&
            object->is<ScriptedDrawable>())
        {
            ensureInputExtras().gamepadScriptedDrawables.push_back(
                object->as<ScriptedDrawable>());
        }
    }
    sortHitComponents();
}

FocusManager* StateMachineInstance::focusManager()
{
    return m_artboardInstance != nullptr ? m_artboardInstance->focusManager()
                                         : nullptr;
}

const FocusManager* StateMachineInstance::focusManager() const
{
    return m_artboardInstance != nullptr ? m_artboardInstance->focusManager()
                                         : nullptr;
}

SMIReporting& StateMachineInstance::ensureReporting()
{
    return *m_reporting.ensureAllocated();
}

SMIBindables& StateMachineInstance::ensureBindables()
{
    return *m_bindables.ensureAllocated();
}

SMIInputExtras& StateMachineInstance::ensureInputExtras()
{
    return *m_inputExtras.ensureAllocated();
}

SMIScripting& StateMachineInstance::ensureScripting()
{
    return *m_scripting.ensureAllocated();
}

SemanticManager* StateMachineInstance::semanticManager() const
{
    auto* extras = inputExtras();
    if (extras == nullptr)
    {
        return nullptr;
    }
    return extras->externalSemanticManager ? extras->externalSemanticManager
                                           : extras->semanticManager.get();
}

ScriptedObject* StateMachineInstance::scriptedObject(
    const ScriptedObject* source) const
{
    auto* scripting = this->scripting();
    return scripting != nullptr ? scripting->find(source) : nullptr;
}

StateMachineInstance::~StateMachineInstance()
{
    m_destroying = true;

    // Clean up semantic tree BEFORE the internal SemanticManager is destroyed.
    // Only needed when we own the manager; if external, the parent cleans up.
    if (auto* extras = inputExtras())
    {
        if (extras->externalSemanticManager == nullptr &&
            extras->semanticManager != nullptr && m_artboardInstance != nullptr)
        {
            m_artboardInstance->cleanupSemanticTree();
        }
        extras->embedderGamepads.clear();
    }

    unbind();
    for (auto inst : m_inputInstances)
    {
        delete inst;
    }
    for (auto& listenerGroup : m_listenerGroups)
    {
        listenerGroup.reset();
    }
    deleteDataBinds();
    delete[] m_layers;
    // What's torn down below can still dirty a bind, which unsettles layers.
    m_layers = nullptr;
    m_anySettled = false;
    // The bindable clones and per-transition property instances are raw-owning,
    // so they are deleted here rather than by the cluster's destructor.
    if (auto* bindables = m_bindables.get())
    {
        bindables->propertyInstances.forEachInstance(
            [](BindableProperty* instance) { delete instance; });
        for (auto& outer : bindables->transitionPropertyInstances)
        {
            for (auto& inner : outer.second)
            {
                delete inner.second;
            }
        }
        bindables->transitionPropertyInstances.clear();
        bindables->propertyInstances.clear();
    }
    if (auto* reporting = this->reporting())
    {
        for (auto& listenerViewModel : reporting->listenerViewModels)
        {
            delete listenerViewModel;
        }
        reporting->listenerViewModels.clear();
    }
    if (auto* scripting = m_scripting.get())
    {
        for (auto& pair : scripting->objects)
        {
            delete pair.second;
        }
        scripting->objects.clear();
    }
}

// When a state machine instanced by a higher level runtime is destroyed, we
// need to clean up all its references from the nested artboard children. The
// reason is that the artboard might still be kept alive and it might have
// invalid pointers. This is not necessary for nested state machines because
// they are destroyed altogether.
void StateMachineInstance::dispose() { removeEventListeners(); }

void StateMachineInstance::removeEventListeners()
{
    if (m_artboardInstance != nullptr)
    {
        for (auto nestedArtboard : m_artboardInstance->nestedArtboards())
        {
            if (nestedArtboard == nullptr)
            {
                continue;
            }
            for (auto animation : nestedArtboard->nestedAnimations())
            {
                if (animation == nullptr)
                {
                    continue;
                }
                if (animation->is<NestedStateMachine>())
                {
                    if (auto notifier = animation->as<NestedStateMachine>()
                                            ->stateMachineInstance())
                    {
                        notifier->removeNestedEventListener(this);
                    }
                }
                else if (animation->is<NestedLinearAnimation>())
                {
                    if (auto notifier = animation->as<NestedLinearAnimation>()
                                            ->animationInstance())
                    {
                        notifier->removeNestedEventListener(this);
                    }
                }
            }
        }
    }
}

#ifdef WITH_RIVE_TOOLS
void StateMachineInstance::onDataBindChanged(DataBindChanged callback)
{
    // dataBinds() is the DataBindContainer base's list — the one addDataBind()
    // actually fills. A same-named member used to shadow it here, and it was
    // never written, so this callback silently never got installed.
    for (auto databind : dataBinds())
    {
        databind->onChanged(callback);
    }
}
#endif

void StateMachineInstance::sortHitComponents()
{
    auto hitShapesCount = m_hitComponents.size();
    auto currentSortedIndex = 0;
    auto count = 0;
    // Since the Artboard is not a drawable, we move all hit components
    // pointing to the artboard to the front of the list
    for (auto& comp : m_hitComponents)
    {
        if (comp->component() != nullptr && comp->component()->is<Artboard>())
        {
            if (currentSortedIndex != count)
            {

                std::iter_swap(m_hitComponents.begin() + currentSortedIndex,
                               m_hitComponents.begin() + count);
            }
            currentSortedIndex++;
        }
        count++;
    }
    Drawable* last = m_artboardInstance->firstDrawable();
    if (last)
    {
        // walk to the end, so we can visit in reverse-order
        while (last->prev)
        {
            last = last->prev;
        }
    }
    for (auto drawable = last; drawable; drawable = drawable->next)
    {
        for (size_t i = currentSortedIndex; i < hitShapesCount; i++)
        {
            if (m_hitComponents[i]->component() == drawable)
            {
                if (currentSortedIndex != i)
                {
                    std::iter_swap(m_hitComponents.begin() + currentSortedIndex,
                                   m_hitComponents.begin() + i);
                }
                currentSortedIndex++;
            }
        }
        if (currentSortedIndex == hitShapesCount)
        {
            break;
        }
    }
}

bool StateMachineInstance::tryChangeState()
{
    updateDataBinds(false);
    bool hasChangedState = false;
    for (size_t i = 0; i < m_layerCount; i++)
    {
        if (m_layers[i].updateState(this))
        {
            hasChangedState = true;
        }
    }
    return hasChangedState;
}

void StateMachineInstance::applyEvents()
{
    auto* reporting = this->reporting();
    if (reporting == nullptr)
    {
        // Nothing has ever reported on this instance, so there is provably
        // nothing to apply and nothing stale to clear.
        return;
    }
    reporting->eventsAppliedDuringLoop.clear();
    int maxIterations = 100;
    int currentIteration = 0;
    while ((reporting->reportedEvents.size() > 0 ||
            reporting->reportedListenerViewModels.size() > 0) &&
           currentIteration++ < maxIterations)
    {
        updateDataBinds(false);
        // The reported/reporting split is load-bearing: notifying below can
        // re-enter reportEvent(), and those events must queue for the next
        // pass rather than mutate the batch being delivered.
        reporting->reportingEvents = reporting->reportedEvents;
        reporting->reportingListenerViewModels =
            reporting->reportedListenerViewModels;
        reporting->reportedEvents.clear();
        reporting->reportedListenerViewModels.clear();
        if (currentIteration > 1)
        {
            // These were reported during the loop, so no host has seen them
            // yet; keep them visible until the next applyEvents.
            reporting->eventsAppliedDuringLoop.insert(
                reporting->eventsAppliedDuringLoop.end(),
                reporting->reportingEvents.begin(),
                reporting->reportingEvents.end());
        }
        this->notifyEventListeners(reporting->reportingEvents, nullptr);
        this->notifyListenerViewModels(reporting->reportingListenerViewModels);
    }
    if (currentIteration >= maxIterations)
    {
        fprintf(stderr,
                "%s StateMachine exceeded max event iterations"
                "on artboard %s\n",
                stateMachine()->name().c_str(),
                artboard()->name().c_str());
    }
}

void StateMachineInstance::setExternalFocusManager(FocusManager* manager)
{
    if (m_artboardInstance != nullptr)
    {
        m_artboardInstance->adoptFocusManager(manager);
    }
}

void StateMachineInstance::enableSemantics()
{
    if (semanticManager() != nullptr)
    {
        return;
    }
    ensureInputExtras().semanticManager = std::make_unique<SemanticManager>();
    if (m_artboardInstance != nullptr)
    {
        m_artboardInstance->buildSemanticTree(semanticManager(), nullptr);
    }
}

void StateMachineInstance::setExternalSemanticManager(
    SemanticManager* manager,
    rcp<SemanticNode> parentNode)
{
    // An unallocated cluster means no external manager is set, so clearing one
    // on such an instance is a no-op — check before allocating.
    auto* existing = inputExtras();
    if ((existing != nullptr ? existing->externalSemanticManager : nullptr) ==
        manager)
    {
        return;
    }
    auto& extras = ensureInputExtras();

    // Clean up the old semantic tree if one was built with a different manager.
    if (m_artboardInstance != nullptr &&
        m_artboardInstance->semanticManager() != nullptr)
    {
        m_artboardInstance->cleanupSemanticTree();
    }

    extras.externalSemanticManager = manager;

    // Rebuild with the new manager. semanticManager() now returns the external
    // manager if set, or the internal one if null.
    if (m_artboardInstance != nullptr)
    {
        m_artboardInstance->buildSemanticTree(semanticManager(), parentNode);
    }
}

void StateMachineInstance::queueFocusEvent(FocusListenerGroup* group,
                                           bool isFocus)
{
    ensureInputExtras().queuedFocusEvents.push_back({group, isFocus});
    markNeedsAdvance();
}

void StateMachineInstance::setFocus(FocusData* focusData)
{
    if (!focusManager())
    {
        return;
    }
    if (focusData != nullptr)
    {
        auto node = focusData->focusNode();
        auto* fm = focusManager();
        fm->setFocus(node);
    }
    else
    {
        focusManager()->clearFocus();
    }
}

StateMachineInstance::FocusState StateMachineInstance::focusState() const
{
    FocusState state;
    const FocusManager* fm = focusManager();
    if (fm == nullptr)
    {
        return state;
    }
    // primaryFocusPtr() avoids a refcount bump on this poll-friendly path.
    FocusNode* focus = fm->primaryFocusPtr();
    if (focus == nullptr)
    {
        return state;
    }
    state.hasFocus = true;
    if (Focusable* focusable = focus->focusable())
    {
        state.expectsKeyboardInput = focusable->acceptsKeyboardInput();
    }
    return state;
}

const Artboard* StateMachineInstance::rootArtboard() const
{
    const Artboard* artboard = m_artboardInstance;
    while (artboard != nullptr && artboard->host() != nullptr &&
           artboard->host()->parentArtboard() != nullptr)
    {
        artboard = artboard->host()->parentArtboard();
    }
    return artboard;
}

void StateMachineInstance::queueFocusTarget(FocusData* focusData)
{
    if (focusData == nullptr)
    {
        return;
    }
    if (!focusManager())
    {
        return;
    }
    focusManager()->requestFocus(focusData->focusNode(), rootArtboard());
    markNeedsAdvance();
}

void StateMachineInstance::queueClearFocus()
{
    if (!focusManager())
    {
        return;
    }
    focusManager()->requestClearFocus(rootArtboard());
    markNeedsAdvance();
}

void StateMachineInstance::queueFocusTraversal(uint32_t traversalKind)
{
    if (!focusManager())
    {
        return;
    }
    focusManager()->requestTraversal(traversalKind, rootArtboard());
    markNeedsAdvance();
}

void StateMachineInstance::processFocusEvents()
{
    auto* extras = inputExtras();
    if (extras == nullptr || extras->queuedFocusEvents.empty())
    {
        return;
    }

    // Moved out before dispatch: a listener action can queue further focus
    // events, and those belong to the next advance, not this drain.
    auto events = std::move(extras->queuedFocusEvents);
    extras->queuedFocusEvents.clear();

    for (const auto& event : events)
    {
        auto listener = event.group->listener();
        bool isFocusEvent = event.isFocus;

        // Match listener type to event type
        if ((isFocusEvent && listener->hasListener(ListenerType::focus)) ||
            (!isFocusEvent && listener->hasListener(ListenerType::blur)))
        {
            listener->performChanges(
                this,
                ListenerInvocation::focus(event.group, event.isFocus));
        }
    }
}

void StateMachineInstance::queueSemanticEvent(SemanticListenerGroup* group,
                                              SemanticActionType actionType)
{
    ensureInputExtras().queuedSemanticEvents.push_back({group, actionType});
    markNeedsAdvance();
}

void StateMachineInstance::processSemanticEvents()
{
    auto* extras = inputExtras();
    if (extras == nullptr || extras->queuedSemanticEvents.empty())
    {
        return;
    }

    auto events = std::move(extras->queuedSemanticEvents);
    extras->queuedSemanticEvents.clear();

    for (const auto& event : events)
    {
        if (event.group == nullptr)
        {
            continue;
        }
        auto* listener = event.group->listener();
        if (listener == nullptr)
        {
            continue;
        }
        listener->performChanges(
            this,
            ListenerInvocation::semantic(event.group, event.actionType));
    }
}

void StateMachineInstance::fireSemanticAction(uint32_t semanticNodeId,
                                              SemanticActionType actionType)
{
    // The unified SemanticManager indexes every SD in the tree — top-level,
    // nested-artboard, and data-bound list items — so this lookup handles
    // all dispatch targets uniformly. SemanticData::fire*() routes the
    // event to listeners, which queue on their own owning state machine.
    auto* mgr = semanticManager();
    if (mgr == nullptr)
    {
        return;
    }
    auto* node = mgr->nodeById(semanticNodeId);
    if (node == nullptr)
    {
        return;
    }
    auto* sd = node->semanticData();
    if (sd == nullptr)
    {
        // Boundary nodes have no owning SemanticData.
        return;
    }
    switch (actionType)
    {
        case SemanticActionType::tap:
            sd->fireSemanticTap();
            break;
        case SemanticActionType::increase:
            sd->fireSemanticIncrease();
            break;
        case SemanticActionType::decrease:
            sd->fireSemanticDecrease();
            break;
    }
}

bool StateMachineInstance::advance(float seconds, bool newFrame)
{
    RIVE_PROF_SCOPE()
    if (m_drawOrderChangeCounter !=
        m_artboardInstance->drawOrderChangeCounter())
    {
        m_drawOrderChangeCounter = m_artboardInstance->drawOrderChangeCounter();
        sortHitComponents();
    }
    if (newFrame)
    {
        processFocusEvents();
        processSemanticEvents();
        applyEvents();
        m_needsAdvance = false;
    }
    updateDataBinds(false);
    for (size_t i = 0; i < m_layerCount; i++)
    {
        if (m_layers[i].advance(this, seconds, newFrame))
        {
            m_needsAdvance = true;
        }
    }

    if (advanceDataBinds(seconds))
    {
        m_needsAdvance = true;
    }

    if (m_inputInstances.size() > 0)
    {
        for (auto inst : m_inputInstances)
        {
            inst->advanced();
        }
    }
    return m_needsAdvance || hasPendingReports();
}

void StateMachineInstance::advancedDataContext()
{
    if (dataBindContext() != nullptr)
    {
        dataBindContext()->advanced();
    }
}

void StateMachineInstance::reset()
{
    advancedDataContext();
    m_artboardInstance->reset();
}

bool StateMachineInstance::advanceAndApply(float seconds)
{
    if (m_artboardInstance->advanceWatermark(seconds))
    {
        // The file's watermark is playing: settle the artboard at time zero so
        // its first frame is ready the instant the watermark ends, but don't
        // let it animate forward. Reporting "keep going" matters here, a false
        // would read as settled and stop the host's ticker mid pre-roll.
        advanceAndApply(0.0f, true);
        return true;
    }
    return advanceAndApply(seconds, true);
}

bool StateMachineInstance::advanceAndApply(float seconds,
                                           bool advanceViewModels)
{
    RIVE_PROF_SCOPE_L(1)
    // Advancing by 0 could return false, when it shouldn't. Force keepGoing
    // to true.
    bool keepGoing = this->advance(seconds, true) || seconds == 0.0f;
    if (focusManager())
    {
        focusManager()->dropFocusIfFocusTargetHidden();
    }
    if (m_artboardInstance->advanceInternal(
            seconds,
            AdvanceFlags::IsRoot | AdvanceFlags::Animate |
                AdvanceFlags::AdvanceNested | AdvanceFlags::NewFrame))
    {
        keepGoing = true;
    }

    for (int outerOptionC = 0; outerOptionC < 5; outerOptionC++)
    {
        if (m_artboardInstance->updatePass(true))
        {
            keepGoing = true;
        }

        // Authoritative drain: updatePass has recomputed renderOpacity and
        // propagated collapse, so target eligibility can be measured against
        // real values. Reaches nested artboards and artboard-component-list
        // items too, since they share this manager. A target can still need
        // several passes to settle, so a request that doesn't take here is
        // kept for the next iteration.
        if (focusManager())
        {
            focusManager()->processPendingFocusRequests(rootArtboard());
            focusManager()->dropFocusIfFocusTargetHidden(rootArtboard());
        }

        // Advance all animations.
        if (this->tryChangeState())
        {
            this->advance(0.0f, false);
            keepGoing = true;
        }

        if (m_artboardInstance->advanceInternal(
                0.0f,
                AdvanceFlags::IsRoot | AdvanceFlags::Animate |
                    AdvanceFlags::AdvanceNested))
        {
            keepGoing = true;
        }
        if (advanceViewModels)
        {
            reset(); // advancedDataContext() (VM consume) + artboard reset
        }
        else
        {
            m_artboardInstance->reset(); // artboard component reset only
        }

        if (!m_artboardInstance->hasDirt(ComponentDirt::Components))
        {
            break;
        }
    }
    // Last chance for this frame: picks up a request queued by the loop's
    // final tryChangeState, and drops anything that still can't take so an
    // unreachable target doesn't leave a request queued indefinitely.
    if (focusManager())
    {
        focusManager()->finishPendingFocusRequests(rootArtboard());
    }
    if (advanceViewModels)
    {
        // Advance detached scripted view models (created via scripts, not part
        // of the bound view model tree) at the end of the frame.
        m_artboardInstance->advanceScriptedViewModels();
    }
    return keepGoing || hasPendingReports();
}

void StateMachineInstance::markNeedsAdvance()
{
    m_needsAdvance = true;
    wakeRow();
}
bool StateMachineInstance::needsAdvance() const { return m_needsAdvance; }

void StateMachineInstance::resetState()
{
    wakeRow();
    for (size_t i = 0; i < m_layerCount; i++)
    {
        m_layers[i].resetState(this);
    }
}

std::string StateMachineInstance::name() const { return m_machine->name(); }

SMIInput* StateMachineInstance::input(size_t index) const
{
    if (index < m_inputInstances.size())
    {
        return m_inputInstances[index];
    }
    return nullptr;
}

template <typename SMType, typename InstType>
InstType* StateMachineInstance::getNamedInput(const std::string& name) const
{
    for (const auto inst : m_inputInstances)
    {
        auto input = inst->input();
        if (input->is<SMType>() && input->name() == name)
        {
            return static_cast<InstType*>(inst);
        }
    }
    return nullptr;
}

SMIBool* StateMachineInstance::getBool(const std::string& name) const
{
    return getNamedInput<StateMachineBool, SMIBool>(name);
}
SMINumber* StateMachineInstance::getNumber(const std::string& name) const
{
    return getNamedInput<StateMachineNumber, SMINumber>(name);
}
SMITrigger* StateMachineInstance::getTrigger(const std::string& name) const
{
    return getNamedInput<StateMachineTrigger, SMITrigger>(name);
}

void StateMachineInstance::setViewModelInstance(
    rcp<ViewModelInstance> viewModelInstance)
{
    if (viewModelInstance == nullptr)
    {
        return;
    }
    if (dataBindContext() == nullptr)
    {
        dataBindContext(make_rcp<DataContext>(viewModelInstance));
        dataBindContext()->addDependentContainer(this);
        return;
    }
    // The data context re-points every attached container (this state machine,
    // the artboard, and any sibling state machines sharing the context) off the
    // old main and onto the new one.
    dataBindContext()->setMainViewModelInstance(viewModelInstance);
}

bool StateMachineInstance::setGlobalViewModelInstance(
    const std::string& name,
    rcp<ViewModelInstance> viewModelInstance)
{
    // A null instance is allowed: it empties the named slot below.
    auto file = m_artboardInstance->file();
    if (file == nullptr)
    {
        return false;
    }
    // The slot is addressed by the named view model (its file index), not by
    // the instance's own view model — so an override instance of a different
    // view model can be placed on the slot.
    uint32_t slotKey = file->viewModelId(name);
    if (slotKey >= file->viewModelCount())
    {
        return false;
    }
    // Only global view models get a slot; a non-global name is not a valid
    // global slot and must not be slotted.
    auto slotViewModel = file->viewModel(slotKey);
    if (slotViewModel == nullptr ||
        static_cast<ViewModelType>(slotViewModel->viewModelType()) !=
            ViewModelType::global)
    {
        return false;
    }
    if (dataBindContext() == nullptr)
    {
        // Nothing to clear when there is no context yet; only create one when
        // actually placing an instance.
        if (viewModelInstance == nullptr)
        {
            return true;
        }
        dataBindContext(make_rcp<DataContext>(rcp<ViewModelInstance>(nullptr)));
        dataBindContext()->addDependentContainer(this);
    }
    // The data context re-points every attached container off any previous
    // instance occupying this slot and onto the new one (or empties the slot
    // when the instance is null).
    dataBindContext()->setViewModelInstanceForSlot(slotKey, viewModelInstance);
    return true;
}

void StateMachineInstance::bind()
{
    if (dataBindContext() == nullptr)
    {
        // No data context yet: create an empty one so the view model
        // instances it needs can be completed on the fly below.
        dataBindContext(make_rcp<DataContext>(rcp<ViewModelInstance>(nullptr)));
        dataBindContext()->addDependentContainer(this);
    }
    // Make sure every view model instance the data context needs exists before
    // it is applied: the main instance plus one for each global view model.
    // Any that are missing are created (completed) on the fly.
    completeViewModelInstances();
    // Apply the current data context: rebind the artboard and state machine
    // data binds in a single pass.
    m_artboardInstance->internalDataContext(dataBindContext());
    internalDataContext(dataBindContext());
}

void StateMachineInstance::completeViewModelInstances()
{
    auto file = m_artboardInstance->file();
    if (file == nullptr)
    {
        return;
    }
    // Ensure a main instance is present. The main is the entry not on the slot
    // keys; if there is none, create the artboard's default and place it first.
    if (dataBindContext()->mainViewModelInstance() == nullptr)
    {
        auto main = file->createDefaultViewModelInstance(m_artboardInstance);
        if (main != nullptr)
        {
            // setMainViewModelInstance re-points every attached container onto
            // the new instance.
            dataBindContext()->setMainViewModelInstance(main);
        }
    }
    // Ensure an instance exists for each global view model slot, creating any
    // missing ones. Occupancy is checked by slot key, so a cross-view-model
    // override already sitting in a slot is not treated as empty.
    for (auto* viewModel : file->globalViewModels())
    {
        uint32_t slotKey = file->viewModelId(viewModel->name());
        if (dataBindContext()->instanceForSlot(slotKey) != nullptr)
        {
            continue;
        }
        auto instance = file->createDefaultViewModelInstance(viewModel);
        if (instance != nullptr)
        {
            // setViewModelInstanceForSlot re-points every attached container
            // onto the new instance.
            dataBindContext()->setViewModelInstanceForSlot(slotKey, instance);
        }
    }
}

void StateMachineInstance::bindViewModelInstance(
    rcp<ViewModelInstance> viewModelInstance)
{
    if (viewModelInstance == nullptr)
    {
        clearDataContext();
        m_artboardInstance->unbind();
        return;
    }
    setViewModelInstance(std::move(viewModelInstance));
    bind();
}

rcp<ViewModelInstance> StateMachineInstance::globalViewModelInstance(
    const std::string& name)
{
    // Pure read: returns the instance in the named slot only if one has been
    // set/bound; never creates.
    if (dataBindContext() == nullptr)
    {
        return nullptr;
    }
    auto file = m_artboardInstance->file();
    if (file == nullptr)
    {
        return nullptr;
    }
    return dataBindContext()->instanceForSlot(file->viewModelId(name));
}

void StateMachineInstance::bindDataContext(rcp<DataContext> dataContext)
{
    clearDataContext();
    dataContext->addDependentContainer(this);
    m_artboardInstance->clearDataContext();
    m_artboardInstance->internalDataContext(dataContext);
    internalDataContext(dataContext);
}

void StateMachineInstance::inheritDataContext(rcp<DataContext> dataContext)
{
    if (dataContext == nullptr)
    {
        return;
    }
    dataContext->addDependentContainer(this);
    internalDataContext(dataContext);
}

void StateMachineInstance::dataContext(rcp<DataContext> dataContext)
{
    clearDataContext();
    internalDataContext(dataContext);
}

void StateMachineInstance::initScriptedObjects()
{
    auto* scripting = m_scripting.get();
    if (scripting == nullptr)
    {
        return;
    }
    for (auto& obj : scripting->objects)
    {
        if (obj.second->scriptAsset() != nullptr)
        {
            if (!obj.second->userLuaInitDone())
            {
                obj.second->scriptAsset()->initScriptedObject(obj.second);
            }
            obj.second->hydrateScriptInputs();
        }
    }
}

void StateMachineInstance::internalDataContext(rcp<DataContext> dataContext)
{
    bindDataBindsFromContext(dataContext);
    if (auto* reporting = this->reporting())
    {
        for (auto listenerViewModel : reporting->listenerViewModels)
        {
            listenerViewModel->bindFromContext(dataContext);
        }
    }
    if (auto* scripting = m_scripting.get())
    {
        for (auto& scriptedObjectItr : scripting->objects)
        {
            scriptedObjectItr.second->dataContext(dataContext);
        }
    }
    initScriptedObjects();
}

void StateMachineInstance::rebind()
{
    m_artboardInstance->clearDataContext();
    m_artboardInstance->internalDataContext(dataBindContext());
    internalDataContext(dataBindContext());
};

void StateMachineInstance::clearDataContext()
{
    if (dataBindContext() != nullptr)
    {
        dataBindContext()->removeDependentContainer(this);
        dataBindContext(nullptr);
    }
    if (auto* reporting = this->reporting())
    {
        for (auto& listenerViewModel : reporting->listenerViewModels)
        {
            listenerViewModel->clearDataContext();
        }
    }
}

void StateMachineInstance::mainViewModelInstanceChanged()
{
    unsettleLayers();
    if (m_artboardInstance != nullptr)
    {
        m_artboardInstance->mainViewModelInstanceChanged();
    }
}

void StateMachineInstance::unsettleLayers()
{
    // Whatever unsettles a layer can change a quiet row too.
    wakeRow();
    if (!m_anySettled)
    {
        return;
    }
    m_anySettled = false;
    for (uint32_t i = 0; i < m_layerCount; i++)
    {
        m_layers[i].unsettle();
    }
}

void StateMachineInstance::wakeRow()
{
    if (m_artboardInstance != nullptr && !m_destroying)
    {
        m_artboardInstance->wakeIfQuietRow();
    }
}

AdvancingComponent::QuietState StateMachineInstance::rowQuietState()
{
    using QuietState = AdvancingComponent::QuietState;
    // Scripts do work this can't see into.
    if (m_scripting.get() != nullptr)
    {
        return QuietState::never;
    }
    // Cheapest first: a busy row is checked again every frame.
    if (m_needsAdvance || m_bindsPending || hasDataBindWork() ||
        m_drawOrderChangeCounter !=
            m_artboardInstance->drawOrderChangeCounter())
    {
        return QuietState::busy;
    }
    if (auto* reporting = this->reporting())
    {
        if (!reporting->reportedEvents.empty() ||
            !reporting->reportingEvents.empty() ||
            !reporting->eventsAppliedDuringLoop.empty() ||
            !reporting->reportedListenerViewModels.empty() ||
            !reporting->reportingListenerViewModels.empty())
        {
            return QuietState::busy;
        }
    }
    if (auto* extras = inputExtras())
    {
        if (!extras->queuedFocusEvents.empty() ||
            !extras->queuedSemanticEvents.empty())
        {
            return QuietState::busy;
        }
    }
    // Every layer frozen, and none changed state last frame (a new frame's
    // advance clears that).
    for (uint32_t i = 0; i < m_layerCount; i++)
    {
        if (!m_layers[i].isFrozen() || m_layers[i].stateChangedOnAdvance())
        {
            return QuietState::busy;
        }
    }
    // Converters that advance can't say whether they would.
    return mayAdvanceDataBinds() ? QuietState::never : QuietState::quiet;
}

void StateMachineInstance::addDirtyDataBind(DataBind* dataBind)
{
    // A bound value is about to change. Layers that settled on the old one
    // must search again, and none may settle until it has been applied. Set
    // before the base, which skips binds already queued or polled.
    m_bindsPending = true;
    unsettleLayers();
    DataBindContainer::addDirtyDataBind(dataBind);
}

void StateMachineInstance::updateDataBinds(bool applyTargetToSource)
{
    // A nested call returns without applying anything, so only the outer pass
    // clears this; binds dirtied while it runs set it again.
    if (!isProcessingDataBinds())
    {
        m_bindsPending = false;
    }
    DataBindContainer::updateDataBinds(applyTargetToSource);
}

void StateMachineInstance::dataContextChanged()
{
    // View model conditions only evaluate with a data context.
    unsettleLayers();
}

void StateMachineInstance::dropInstanceValueBindsTargeting(Core* target)
{
    DataBindContainer::dropInstanceValueBindsTargeting(target);
    if (m_artboardInstance != nullptr)
    {
        m_artboardInstance->dropInstanceValueBindsTargeting(target);
    }
}

void StateMachineInstance::relinkDataContext()
{
    unsettleLayers();
    m_artboardInstance->relinkDataContext();
}

void StateMachineInstance::rebuildDataBind(DataBind* dataBind)
{
    if (dataBind->is<DataBindContext>())
    {
        dataBind->as<DataBindContext>()->bindFromContext(
            dataBindContext().get());
    }
};

void StateMachineInstance::unbind()
{
    clearDataContext();
    unbindDataBinds();
}

size_t StateMachineInstance::stateChangedCount() const
{
    size_t count = 0;
    for (size_t i = 0; i < m_layerCount; i++)
    {
        if (m_layers[i].stateChangedOnAdvance())
        {
            count++;
        }
    }
    return count;
}

const LayerState* StateMachineInstance::stateChangedByIndex(size_t index) const
{
    size_t count = 0;
    for (size_t i = 0; i < m_layerCount; i++)
    {
        if (m_layers[i].stateChangedOnAdvance())
        {
            if (count == index)
            {
                return m_layers[i].currentState();
            }
            count++;
        }
    }
    return nullptr;
}

size_t StateMachineInstance::currentAnimationCount() const
{
    size_t count = 0;
    for (size_t i = 0; i < m_layerCount; i++)
    {
        if (m_layers[i].currentAnimation() != nullptr)
        {
            count++;
        }
    }
    return count;
}

const LinearAnimationInstance* StateMachineInstance::currentAnimationByIndex(
    size_t index) const
{
    size_t count = 0;
    for (size_t i = 0; i < m_layerCount; i++)
    {
        if (m_layers[i].currentAnimation() != nullptr)
        {
            if (count == index)
            {
                return m_layers[i].currentAnimation();
            }
            count++;
        }
    }
    return nullptr;
}

bool StateMachineInstance::hasPendingReports() const
{
    auto* reporting = this->reporting();
    return reporting != nullptr &&
           (!reporting->reportedEvents.empty() ||
            !reporting->reportedListenerViewModels.empty());
}

void StateMachineInstance::reportEvent(Event* event, float delaySeconds)
{
    wakeRow();
    ensureReporting().reportedEvents.push_back(
        EventReport(event, delaySeconds));
}

void StateMachineInstance::reportListenerViewModel(
    ListenerViewModel* listenerViewModel)
{
    wakeRow();
    ensureReporting().reportedListenerViewModels.push_back(listenerViewModel);
}

std::size_t StateMachineInstance::reportedEventCount() const
{
    auto* reporting = this->reporting();
    if (reporting == nullptr)
    {
        return 0;
    }
    return reporting->eventsAppliedDuringLoop.size() +
           reporting->reportedEvents.size();
}

const EventReport StateMachineInstance::reportedEventAt(std::size_t index) const
{
    auto* reporting = this->reporting();
    if (reporting == nullptr)
    {
        return EventReport(nullptr, 0.0f);
    }
    if (index < reporting->eventsAppliedDuringLoop.size())
    {
        return reporting->eventsAppliedDuringLoop[index];
    }
    index -= reporting->eventsAppliedDuringLoop.size();
    if (index >= reporting->reportedEvents.size())
    {
        return EventReport(nullptr, 0.0f);
    }
    return reporting->reportedEvents[index];
}

void StateMachineInstance::notify(const std::vector<EventReport>& events,
                                  NestedArtboard* context)
{
    notifyEventListeners(events, context);
    updateDataBinds(false);
}

void StateMachineInstance::notifyListenerViewModels(
    const std::vector<ListenerViewModel*>& events)
{
    if (events.size() > 0)
    {
        for (auto& listenerViewModel : events)
        {
            listenerViewModel->listener()->performChanges(
                this,
                ListenerInvocation::viewModelChange(listenerViewModel));
        }
    }
}

void StateMachineInstance::notifyEventListeners(
    const std::vector<EventReport>& events,
    NestedArtboard* source)
{
    if (events.size() > 0)
    {
        // We trigger the listeners in order
        for (size_t i = 0; i < m_machine->listenerCount(); i++)
        {
            auto listener = m_machine->listener(i);
            auto target = artboard()->resolve(listener->targetId());
            if (listener != nullptr &&
                listener->hasListener(ListenerType::event) &&
                (source == nullptr || source == target))
            {
                for (const auto event : events)
                {
                    auto sourceArtboard = source == nullptr
                                              ? artboard()
                                              : source->artboardInstance();

                    // NOTE: this issue can't happen anymore because a new
                    // fix in the editor prevents selecting other artboard
                    // as target. But the fix is kept here to fix older
                    // files. listener->eventId() can point to an id from an
                    // event in the context of this artboard or the
                    // context of a nested artboard. Because those ids
                    // belong to different contexts, they can have the
                    // same value. So when the eventId is resolved
                    // within one context, but actually pointing to the
                    // other, it can return the wrong event object. If,
                    // by chance, that event exists in the other
                    // context, and is being reported, it will trigger
                    // the wrong set of actions. This validation makes
                    // sure that a listener must be targetting the
                    // current artboard to disambiguate between external
                    // and internal events.
                    if (source == nullptr)
                    {
                        auto target =
                            sourceArtboard->resolve(listener->targetId());
                        if (target && target != artboard() &&
                            !target->is<Event>())
                        {
                            continue;
                        }
                    }
                    if (listener->is<StateMachineListenerSingle>())
                    {
                        auto listenerEvent = sourceArtboard->resolve(
                            listener->as<StateMachineListenerSingle>()
                                ->eventId());
                        if (listenerEvent == event.event())
                        {
                            listener->performChanges(
                                this,
                                ListenerInvocation::reportedEvent(
                                    event.event(),
                                    event.secondsDelay()));
                            break;
                        }
                    }
                    else
                    {
                        size_t index = 0;
                        while (index < listener->listenerInputTypeCount())
                        {
                            auto listenerInputType =
                                listener->listenerInputType(index);
                            if (listenerInputType->is<ListenerInputTypeEvent>())
                            {

                                auto listenerInputTypeEvent =
                                    listenerInputType
                                        ->as<ListenerInputTypeEvent>();
                                auto listenerEvent = sourceArtboard->resolve(
                                    listenerInputTypeEvent->eventId());
                                if (listenerEvent == event.event())
                                {
                                    listener->performChanges(
                                        this,
                                        ListenerInvocation::reportedEvent(
                                            event.event(),
                                            event.secondsDelay()));
                                    break;
                                }
                            }
                            index += 1;
                        }
                    }
                }
            }
        }
        // Bubble the event up to parent artboard state machines
        // immediately
        for (auto listener : nestedEventListeners())
        {
            listener->notify(events, nestedArtboard());
        }

        for (auto report : events)
        {
            auto event = report.event();
            if (event->is<AudioEvent>())
            {
                event->as<AudioEvent>()->play();
            }
        }
    }
}

void StateMachineInstance::enablePointerEvents(int pointerId)
{
    for (const auto& hitShape : m_hitComponents)
    {
        hitShape->enablePointerEvents(pointerId);
    }
}

void StateMachineInstance::disablePointerEvents(int pointerId)
{
    for (const auto& hitShape : m_hitComponents)
    {
        hitShape->disablePointerEvents(pointerId);
    }
}

BindableProperty* StateMachineInstance::bindablePropertyInstance(
    BindableProperty* bindableProperty) const
{
    auto* bindables = this->bindables();
    if (bindables == nullptr)
    {
        return nullptr;
    }
    return bindables->propertyInstances.find(bindableProperty);
}

DataBind* StateMachineInstance::bindableDataBindToSource(
    BindableProperty* bindableProperty) const
{
    auto* bindables = this->bindables();
    if (bindables == nullptr)
    {
        return nullptr;
    }
    auto dataBind = bindables->dataBindsToSource.find(bindableProperty);
    if (dataBind == bindables->dataBindsToSource.end())
    {
        return nullptr;
    }
    return dataBind->second;
}

DataBind* StateMachineInstance::bindableDataBindToTarget(
    BindableProperty* bindableProperty) const
{
    auto* bindables = this->bindables();
    if (bindables == nullptr)
    {
        return nullptr;
    }
    auto dataBind = bindables->dataBindsToTarget.find(bindableProperty);
    if (dataBind == bindables->dataBindsToTarget.end())
    {
        return nullptr;
    }
    return dataBind->second;
}

BindablePropertyNumber* StateMachineInstance::findTransitionPropertyInstance(
    const StateTransition* transition,
    uint32_t propertyKey) const
{
    auto* bindables = this->bindables();
    if (bindables == nullptr)
    {
        return nullptr;
    }
    auto it = bindables->transitionPropertyInstances.find(transition);
    if (it != bindables->transitionPropertyInstances.end())
    {
        auto propIt = it->second.find(propertyKey);
        if (propIt != it->second.end())
        {
            return propIt->second;
        }
    }
    return nullptr;
}

bool StateMachineInstance::hasFocusNodes()
{
    if (!focusManager())
    {
        return false;
    }
    auto* fm = focusManager();
    return fm->hasFocusableContent();
}

bool StateMachineInstance::focusNext()
{
    if (!focusManager())
    {
        return false;
    }
    auto* fm = focusManager();
    return fm->focusNext();
}

bool StateMachineInstance::focusPrevious()
{
    if (!focusManager())
    {
        return false;
    }
    auto* fm = focusManager();
    return fm->focusPrevious();
}

bool StateMachineInstance::focusLeft()
{
    if (!focusManager())
    {
        return false;
    }
    auto* fm = focusManager();
    return fm->focusLeft();
}

bool StateMachineInstance::focusRight()
{
    if (!focusManager())
    {
        return false;
    }
    auto* fm = focusManager();
    return fm->focusRight();
}

bool StateMachineInstance::focusUp()
{
    if (!focusManager())
    {
        return false;
    }
    auto* fm = focusManager();
    return fm->focusUp();
}

bool StateMachineInstance::focusDown()
{
    if (!focusManager())
    {
        return false;
    }
    auto* fm = focusManager();
    return fm->focusDown();
}

void StateMachineInstance::clearFocus()
{
    if (!focusManager())
    {
        return;
    }
    auto* fm = focusManager();
    fm->clearFocus();
}

bool StateMachineInstance::keyInput(Key key,
                                    KeyModifiers modifiers,
                                    bool isPressed,
                                    bool isRepeat)
{
    if (!focusManager())
    {
        return false;
    }
    auto* fm = focusManager();
    return fm->keyInput(key, modifiers, isPressed, isRepeat);
}

bool StateMachineInstance::textInput(const std::string& text)
{
    if (!focusManager())
    {
        return false;
    }
    auto* fm = focusManager();
    return fm->textInput(text);
}

std::string StateMachineInstance::selectedText() const
{
    const auto* fm = focusManager();
    if (!fm)
    {
        return "";
    }
    return fm->selectedText();
}
