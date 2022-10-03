#ifndef _RIVE_STATE_MACHINE_LAYER_HPP_
#define _RIVE_STATE_MACHINE_LAYER_HPP_
#include "rive/generated/animation/state_machine_layer_base.hpp"
#include <stdio.h>
#include <vector>

namespace rive
{
class LayerState;
class StateMachineLayerImporter;
class AnyState;
class EntryState;
class ExitState;
class StateMachineLayer : public StateMachineLayerBase
{
    friend class StateMachineLayerImporter;

private:
    std::vector<LayerState*> m_States;
    AnyState* m_Any = nullptr;
    EntryState* m_Entry = nullptr;
    ExitState* m_Exit = nullptr;

    void addState(LayerState* state);

public:
    ~StateMachineLayer() override;
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;

    StatusCode import(ImportStack& importStack) override;

    const AnyState* anyState() const { return m_Any; }
    const EntryState* entryState() const { return m_Entry; }
    const ExitState* exitState() const { return m_Exit; }

#ifdef TESTING
    size_t stateCount() const { return m_States.size(); }
    LayerState* state(size_t index) const
    {
        if (index < m_States.size())
        {
            return m_States[index];
        }
        return nullptr;
    }
#endif
};
} // namespace rive

#endif