#ifndef _RIVE_LAYER_STATE_HPP_
#define _RIVE_LAYER_STATE_HPP_
#include "rive/generated/animation/layer_state_base.hpp"
#include <stdio.h>
#include <vector>

namespace rive
{
class ArtboardInstance;
class StateTransition;
class LayerStateImporter;
class StateMachineLayerImporter;
class StateInstance;

class LayerState : public LayerStateBase
{
    friend class LayerStateImporter;
    friend class StateMachineLayerImporter;

private:
    std::vector<StateTransition*> m_Transitions;
    void addTransition(StateTransition* transition);

public:
    ~LayerState() override;
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;

    StatusCode import(ImportStack& importStack) override;

    size_t transitionCount() const
    {
#ifdef WITH_RIVE_EDITOR
        if (m_Transitions.empty())
        {
            return m_editorTransitions.size();
        }
#endif
        return m_Transitions.size();
    }
    StateTransition* transition(size_t index) const
    {
        if (index < m_Transitions.size())
        {
            return m_Transitions[index];
        }
#ifdef WITH_RIVE_EDITOR
        if (m_Transitions.empty() && index < m_editorTransitions.size())
        {
            return m_editorTransitions[index];
        }
#endif
        return nullptr;
    }

    /// Make an instance of this state that can be advanced and applied by
    /// the state machine when it is active or being transitioned from.
    virtual std::unique_ptr<StateInstance> makeInstance(
        ArtboardInstance* instance) const;

    /// Whether every condition on this state's transitions only reads values
    /// whose changes reach the state machine instance (view model values it
    /// binds, and its data context). A layer whose search found nothing from
    /// such a state can skip searching again until one of them changes. Set
    /// once by StateMachine::onAddedClean; never set in editor builds, whose
    /// transitions change after import.
    bool transitionsSettleSafe() const { return m_transitionsSettleSafe; }
    void transitionsSettleSafe(bool value) { m_transitionsSettleSafe = value; }

    /// Whether none of this state's transitions waits on an exit time, the only
    /// thing that reads how long the state has been active. A settled layer in
    /// such a state that keys nothing can skip advancing it. Set with
    /// transitionsSettleSafe.
    bool transitionsIgnoreTime() const { return m_transitionsIgnoreTime; }
    void transitionsIgnoreTime(bool value) { m_transitionsIgnoreTime = value; }

#ifdef WITH_RIVE_EDITOR
    // Editor-only parallel non-owning transition list. `m_Transitions`
    // is owned by `LayerState::~LayerState`. See
    // `StateMachineLayer::m_editorStates` for the pattern rationale.
    void addTransitionForEditor(StateTransition* transition);
    void clearEditorTransitions();
    size_t editorTransitionCount() const { return m_editorTransitions.size(); }
#endif

private:
#ifdef WITH_RIVE_EDITOR
    std::vector<StateTransition*> m_editorTransitions;
#endif
    bool m_transitionsSettleSafe = false;
    bool m_transitionsIgnoreTime = false;
};
} // namespace rive

#endif