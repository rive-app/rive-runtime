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

    size_t stateCount() const
    {
#ifdef WITH_RIVE_EDITOR
        if (m_States.empty())
        {
            return m_editorStates.size();
        }
#endif
        return m_States.size();
    }
    LayerState* state(size_t index) const
    {
        if (index < m_States.size())
        {
            return m_States[index];
        }
#ifdef WITH_RIVE_EDITOR
        if (m_States.empty() && index < m_editorStates.size())
        {
            return m_editorStates[index];
        }
#endif
        return nullptr;
    }

#ifdef WITH_RIVE_EDITOR
    // Editor-only parallel non-owning state list. `m_States` is
    // owned (see `StateMachineLayer::~StateMachineLayer`), so coop-
    // hydrated LayerStates (owned by `EditorFile::m_arena`) cannot
    // share that list. Special states (Any/Entry/Exit) are slotted
    // into the corresponding pointer members during
    // `addStateForEditor`, mirroring `onAddedDirty`.
    void addStateForEditor(LayerState* state);
    void clearEditorStates();
    size_t editorStateCount() const { return m_editorStates.size(); }
#endif

private:
#ifdef WITH_RIVE_EDITOR
    std::vector<LayerState*> m_editorStates;
#endif
};
} // namespace rive

#endif