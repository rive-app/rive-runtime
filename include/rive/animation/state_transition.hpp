#ifndef _RIVE_STATE_TRANSITION_HPP_
#define _RIVE_STATE_TRANSITION_HPP_
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/animation/state_transition_flags.hpp"
#include "rive/generated/animation/state_transition_base.hpp"
#ifdef WITH_RIVE_EDITOR
#include "rive/editor/object_arena.hpp"
#endif
#include <stdio.h>
#include <vector>

namespace rive
{
class LayerState;
class StateMachineLayerImporter;
class StateTransitionImporter;
class TransitionCondition;
class StateInstance;
class StateMachineInstance;
class StateMachineLayerInstance;
class LinearAnimation;
class LinearAnimationInstance;

enum class AllowTransition : unsigned char
{
    no,
    waitingForExit,
    yes
};

class StateTransition : public StateTransitionBase
{
    friend class StateMachineLayerImporter;
    friend class StateTransitionImporter;

private:
    StateTransitionFlags transitionFlags() const
    {
        return static_cast<StateTransitionFlags>(flags());
    }
    LayerState* m_StateTo = nullptr;
    uint32_t m_EvaluatedRandomWeight = 1;
    KeyFrameInterpolator* m_Interpolator = nullptr;
#ifdef WITH_RIVE_EDITOR
    // Slice 6 Phase E dual-storage. See targeted_constraint.hpp.
    CoreHandle m_StateToHandle;
    CoreHandle m_InterpolatorHandle;
#endif

    std::vector<TransitionCondition*> m_Conditions;
    void addCondition(TransitionCondition* condition);

public:
    ~StateTransition() override;
#ifdef WITH_RIVE_EDITOR
    // Bodies in editor_native/native/src/editor/animation/sm/
    // state_transition_editor.cpp.
    const LayerState* stateTo() const;
    KeyFrameInterpolator* interpolator() const;
#else
    inline const LayerState* stateTo() const { return m_StateTo; }
    inline KeyFrameInterpolator* interpolator() const { return m_Interpolator; }
#endif

    inline uint32_t evaluatedRandomWeight() const
    {
        return m_EvaluatedRandomWeight;
    }
    void evaluatedRandomWeight(uint32_t value)
    {
        m_EvaluatedRandomWeight = value;
    }

    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;

    /// Whether the transition is marked disabled (usually done in the
    /// editor).
    bool isDisabled() const
    {
        return (transitionFlags() & StateTransitionFlags::Disabled) ==
               StateTransitionFlags::Disabled;
    }

    /// Returns AllowTransition::yes when this transition can be taken from
    /// stateFrom with the given inputs.
    AllowTransition allowed(StateInstance* stateFrom,
                            StateMachineInstance* stateMachineInstance,
                            StateMachineLayerInstance* layerInstance) const;

    /// Whether the animation is held at exit or if it keeps advancing
    /// during mixing.
    bool pauseOnExit() const
    {
        return (transitionFlags() & StateTransitionFlags::PauseOnExit) ==
               StateTransitionFlags::PauseOnExit;
    }

    /// Whether exit time is enabled. All other conditions still apply, the
    /// exit time is effectively an AND with the rest of the conditions.
    bool enableExitTime() const
    {
        return (transitionFlags() & StateTransitionFlags::EnableExitTime) ==
               StateTransitionFlags::EnableExitTime;
    }

    /// Whether the transition can be interrupted.
    bool enableEarlyExit() const
    {
        return (transitionFlags() & StateTransitionFlags::EnableEarlyExit) ==
               StateTransitionFlags::EnableEarlyExit;
    }

    bool durationIsPercentage() const
    {
        return (transitionFlags() &
                StateTransitionFlags::DurationIsPercentage) ==
               StateTransitionFlags::DurationIsPercentage;
    }

    StatusCode import(ImportStack& importStack) override;

    size_t conditionCount() const
    {
#ifdef WITH_RIVE_EDITOR
        if (m_Conditions.empty())
        {
            return m_editorConditions.size();
        }
#endif
        return m_Conditions.size();
    }
    TransitionCondition* condition(size_t index) const
    {
        if (index < m_Conditions.size())
        {
            return m_Conditions[index];
        }
#ifdef WITH_RIVE_EDITOR
        if (m_Conditions.empty() && index < m_editorConditions.size())
        {
            return m_editorConditions[index];
        }
#endif
        return nullptr;
    }

#ifdef WITH_RIVE_EDITOR
    // Editor-only parallel non-owning condition list (see
    // `StateMachineLayer::m_editorStates` for pattern rationale).
    // The runtime importer's resolve step (`StateMachineLayerImporter::
    // resolve`) sets `m_StateTo` from the layer-relative `stateToId`
    // index and `m_Interpolator` from `interpolatorId`. Coop bypasses
    // that path; finalizeBatch resolves both via `EditorFile::resolve`
    // (CoopId map) and pokes the values in directly.
    void addConditionForEditor(TransitionCondition* condition);
    void clearEditorConditions();
    size_t editorConditionCount() const { return m_editorConditions.size(); }
    // Bodies in state_transition_editor.cpp — dispatch through
    // editorArena() so editor-flow Cores get the handle path while
    // the runtime importer (which still assigns m_StateTo directly
    // via friend class access) keeps the raw fallback.
    void setStateToForEditor(LayerState* state);
    void setInterpolatorForEditor(KeyFrameInterpolator* interp);
#endif

private:
#ifdef WITH_RIVE_EDITOR
    std::vector<TransitionCondition*> m_editorConditions;
#endif

public:
    /// The amount of time to mix the outgoing animation onto the incoming
    /// one when changing state. Only applies when going out from an
    /// AnimationState.
    float mixTime(const LayerState* stateFrom) const;

    /// Computes the exit time in seconds of the stateFrom. Set absolute to
    /// true if you want the returned time to be relative to the entire
    /// animation. Set absolute to false if you want it relative to the work
    /// area.
    float exitTimeSeconds(const LayerState* stateFrom,
                          bool absolute = false) const;

    /// Provide the animation instance to use for computing percentage
    /// durations for exit time.
    virtual const LinearAnimationInstance* exitTimeAnimationInstance(
        const StateInstance* from) const;

    /// Provide the animation to use for computing percentage durations for
    /// exit time.
    virtual const LinearAnimation* exitTimeAnimation(
        const LayerState* from) const;

    /// Retruns true when we need to hold the exit time, also applies the
    /// correct time to the animation instance in the stateFrom, when
    /// applicable (when it's an AnimationState).
    bool applyExitCondition(StateInstance* stateFrom) const;

    /// Marks any trigger based condition as used for this layer
    void useLayerInConditions(StateMachineInstance* stateMachineInstance,
                              StateMachineLayerInstance* layerInstance) const;
};
} // namespace rive

#endif