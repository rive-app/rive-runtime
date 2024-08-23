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

    size_t transitionCount() const { return m_Transitions.size(); }
    StateTransition* transition(size_t index) const
    {
        if (index < m_Transitions.size())
        {
            return m_Transitions[index];
        }
        return nullptr;
    }

    /// Make an instance of this state that can be advanced and applied by
    /// the state machine when it is active or being transitioned from.
    virtual std::unique_ptr<StateInstance> makeInstance(ArtboardInstance* instance) const;
};
} // namespace rive

#endif