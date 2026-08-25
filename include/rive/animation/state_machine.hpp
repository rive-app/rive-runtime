#ifndef _RIVE_STATE_MACHINE_HPP_
#define _RIVE_STATE_MACHINE_HPP_
#include "rive/generated/animation/state_machine_base.hpp"
#include <stdio.h>
#include <vector>

namespace rive
{
class StateMachineLayer;
class StateMachineInput;
class StateMachineListener;
class StateMachineImporter;
class ScriptedObject;
class DataBind;
class StateMachine : public StateMachineBase
{
    friend class StateMachineImporter;

private:
    std::vector<std::unique_ptr<StateMachineLayer>> m_Layers;
    std::vector<std::unique_ptr<StateMachineInput>> m_Inputs;
    std::vector<std::unique_ptr<StateMachineListener>> m_Listeners;
    std::vector<std::unique_ptr<DataBind>> m_dataBinds;
    std::vector<ScriptedObject*> m_scriptedObjects;

    void addLayer(std::unique_ptr<StateMachineLayer>);
    void addInput(std::unique_ptr<StateMachineInput>);
    void addListener(std::unique_ptr<StateMachineListener>);
    void addDataBind(std::unique_ptr<DataBind>);

public:
    StateMachine();
    ~StateMachine() override;

    StatusCode import(ImportStack& importStack) override;

    size_t layerCount() const
    {
#ifdef WITH_RIVE_EDITOR
        if (m_Layers.empty())
        {
            return m_editorLayers.size();
        }
#endif
        return m_Layers.size();
    }
    size_t inputCount() const
    {
#ifdef WITH_RIVE_EDITOR
        if (m_Inputs.empty())
        {
            return m_editorInputs.size();
        }
#endif
        return m_Inputs.size();
    }
    size_t listenerCount() const
    {
#ifdef WITH_RIVE_EDITOR
        if (m_Listeners.empty())
        {
            return m_editorListeners.size();
        }
#endif
        return m_Listeners.size();
    }
    size_t dataBindCount() const
    {
#ifdef WITH_RIVE_EDITOR
        if (m_dataBinds.empty())
        {
            return m_editorDataBinds.size();
        }
#endif
        return m_dataBinds.size();
    }
    void addScriptedObject(ScriptedObject* object);
    std::vector<ScriptedObject*> scriptedObjects() const
    {
        return m_scriptedObjects;
    }

    const StateMachineInput* input(std::string name) const;
    const StateMachineInput* input(size_t index) const;
    const StateMachineLayer* layer(std::string name) const;
    const StateMachineLayer* layer(size_t index) const;
    const DataBind* dataBind(size_t index) const;
    const StateMachineListener* listener(size_t index) const;

    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;

#ifdef WITH_RIVE_EDITOR
    // Editor-only parallel non-owning lists. Coop hydration delivers
    // layers/inputs/listeners as `Core*`s into `EditorFile::m_arena`
    // (which owns them); these mirror the runtime owning lists so
    // `StateMachineInstance::init` can read whichever is populated.
    // See `LinearAnimation::m_EditorKeyedObjects` for the pattern
    // rationale.
    void addLayerForEditor(StateMachineLayer* layer);
    void addInputForEditor(StateMachineInput* input);
    void addListenerForEditor(StateMachineListener* listener);
    void addDataBindForEditor(DataBind* dataBind);
    void clearEditorLayers();
    void clearEditorInputs();
    void clearEditorListeners();
    void clearEditorDataBinds();
    size_t editorLayerCount() const { return m_editorLayers.size(); }
    size_t editorInputCount() const { return m_editorInputs.size(); }
    size_t editorListenerCount() const { return m_editorListeners.size(); }
    size_t editorDataBindCount() const { return m_editorDataBinds.size(); }
#endif

private:
#ifdef WITH_RIVE_EDITOR
    std::vector<StateMachineLayer*> m_editorLayers;
    std::vector<StateMachineInput*> m_editorInputs;
    std::vector<StateMachineListener*> m_editorListeners;
    // StateMachine's runtime `m_dataBinds` is `unique_ptr`-owning;
    // coop-hydrated DataBinds live in `EditorFile::m_arena` so we
    // can't share that list without surrendering ownership. This
    // parallel non-owning list mirrors the SM-1 layer/state pattern.
    // Read-side dispatch in `dataBindCount` / `dataBind(i)` walks
    // it when the runtime list is empty.
    std::vector<DataBind*> m_editorDataBinds;
#endif
};
} // namespace rive

#endif