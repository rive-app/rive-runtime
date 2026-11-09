#ifndef _RIVE_STATE_MACHINE_HPP_
#define _RIVE_STATE_MACHINE_HPP_
#include "rive/generated/animation/state_machine_base.hpp"
#include "rive/animation/blend_accumulator.hpp"
#include <atomic>
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
class LayerState;
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
  void addScriptedInterpolators(const LayerState* state);

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
  bool hasScriptedObjects() const { return !m_scriptedObjects.empty(); }

  const StateMachineInput* input(std::string name) const;
  const StateMachineInput* input(size_t index) const;
  const StateMachineLayer* layer(std::string name) const;
  const StateMachineLayer* layer(size_t index) const;
  const DataBind* dataBind(size_t index) const;
  const StateMachineListener* listener(size_t index) const;

  StatusCode onAddedDirty(CoreContext* context) override;
  StatusCode onAddedClean(CoreContext* context) override;

  // Whether a transition condition compares a component's trigger
  // (TransitionViewModelCondition::readsComponentTrigger). Editor builds
  // don't classify, since conditions there change after import.
  bool readsComponentTriggers() const
  {
#ifdef WITH_RIVE_EDITOR
    return true;
#else
    return m_readsComponentTriggers;
#endif
  }

  // Whether a transition condition reads whether a view model change is
  // pending (TransitionViewModelCondition::readsPendingChange), so its
  // instances keep a change window per layer. Editor builds don't classify,
  // since conditions there change after import.
  bool readsPendingChanges() const
  {
#ifdef WITH_RIVE_EDITOR
    return true;
#else
    return m_readsPendingChanges;
#endif
  }

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

#ifndef WITH_RIVE_EDITOR
  // A property that states in two or more of this machine's layers key.
  // Each layer would set it in turn every frame, so its instances collect
  // these in one accumulator and set each once, to its final value (see
  // StateMachineInstance). Only properties whose setters do no more than
  // invalidate are listed (see layerAccumulatedKind in the .cpp).
  struct LayerConflict
  {
    uint32_t objectId;
    uint16_t propertyKey;
    BlendAccumulator::Kind kind;
  };
  // Worked out on first use, once every importer has resolved, and shared
  // by every instance (on any thread) from then on.
  const std::vector<LayerConflict>& layerConflicts() const;

  // For each of this machine's data binds, the index of an earlier one whose
  // bindable property its instances share instead of cloning their own, or
  // -1. Empty when no bind has one. Every condition comparator owns its
  // bindable, so a machine whose conditions read one value N times would
  // otherwise bind N copies of it per instance, each a dependent of the
  // value that a change dirties. Two binds share when their bindables always
  // hold the same value: plain source-to-target binds with no converter,
  // from the same path, into bindables of the same type and default value
  // that no other bind writes. Triggers don't share: a layer that takes a
  // transition on one marks it used. Worked out on first use, like
  // layerConflicts.
  const std::vector<int32_t>& bindableTwins() const;
#endif

private:
#ifndef WITH_RIVE_EDITOR
  bool m_readsComponentTriggers = false;
  bool m_readsPendingChanges = false;
  mutable std::atomic<bool> m_layerConflictsReady{false};
  mutable std::vector<LayerConflict> m_layerConflicts;
  mutable std::atomic<bool> m_bindableTwinsReady{false};
  mutable std::vector<int32_t> m_bindableTwins;
#endif
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