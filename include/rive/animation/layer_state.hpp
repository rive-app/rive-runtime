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
};
} // namespace rive

#endif