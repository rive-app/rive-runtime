#ifndef _RIVE_SCRIPTED_INTERPOLATOR_HPP_
#define _RIVE_SCRIPTED_INTERPOLATOR_HPP_
#include "rive/generated/scripted/scripted_interpolator_base.hpp"
#include "rive/scripted/scripted_object.hpp"
#include <stdio.h>
#include <memory>
#include <vector>
namespace rive
{
class DataBind;
class DataBindContainer;
class DataContext;
class StatefulScriptedInterpolatorBinds;
class ScriptedInterpolator : public ScriptedInterpolatorBase,
                             public ScriptedObject
{
public:
#ifdef WITH_RIVE_EDITOR
  void editorPrepareForDestruction() override;
#endif
  float transformValue(float valueFrom, float valueTo, float factor) override;
  float transform(float factor) const override;
  uint32_t assetId() override { return scriptAssetId(); }
  bool addScriptedDirt(ComponentDirt value, bool recurse = false) override
  {
    return false;
  }
  ScriptProtocol scriptProtocol() override
  {
    return ScriptProtocol::interpolator;
  }
  Component* component() override { return nullptr; }

  /// Not a Component, so Artboard::m_Objects never takes this object's
  /// ScriptInputs and it has to free them itself -- the same rule
  /// ScriptedDataConverter, ScriptedListenerAction and
  /// ScriptedTransitionCondition already implement. Like them, the
  /// destructor calls it: ~ScriptedObject cannot, because a virtual call
  /// from a base destructor resolves to the base override.
  ~ScriptedInterpolator();
  void disposeScriptInputs() override;
  StatusCode import(ImportStack& importStack) override;

  // Cloning support so each (LinearAnimationInstance, InterpolatingKeyFrame)
  // pair can vend its own stateful Lua instance. Mirrors
  // ScriptedListenerAction / ScriptedTransitionCondition. Without these the
  // base `ScriptedObject::cloneScriptedObject` returns nullptr and the
  // per-instance path silently degrades to the shared template's
  // identity/linear fallback.
  Core* clone() const override;
  ScriptedObject* cloneScriptedObject(DataBindContainer*) const override;
  void addProperty(CustomProperty* prop) override;
};

/// One live (stateful) clone of a shared ScriptedInterpolator template, plus
/// the input data binds the clone parked on a DataBindContainer.
///
/// Every consumer that runs a scripted interpolator owns one of these per
/// (runtime owner, authored interpolator): a LinearAnimationInstance per
/// keyframe, an ArtboardInstance per layout style interpolator, a
/// StateMachineInstance per transition interpolator, a data converter clone
/// per converter. Layout children that inherit an interpolator borrow their
/// ancestor's clone rather than owning one.
///
/// cloneProperties() appends the clone's input binds to a container, and their
/// targets are CustomPropertys the clone owns. The destructor therefore pulls
/// the binds out of the container and deletes them BEFORE it destroys the
/// clone.
class StatefulScriptedInterpolator
{
public:
  /// Clones `source`, parking its input binds on `container`, which updates
  /// and rebinds them with its own and must outlive this object. get() is
  /// null when no clone could be vended (e.g. scripting compiled out of this
  /// runtime).
  StatefulScriptedInterpolator(
      const ScriptedInterpolator* source,
      DataBindContainer* container,
      const rcp<DataContext>& dataContext);

  /// Clones `source`, keeping its input binds in a container of its own. For
  /// an owner whose own bind list must not see them -- a data converter
  /// copies its binds when it is cloned. The owner forwards
  /// bindFromContext() and updateDataBinds().
  StatefulScriptedInterpolator(
      const ScriptedInterpolator* source,
      const rcp<DataContext>& dataContext);

  ~StatefulScriptedInterpolator();

  StatefulScriptedInterpolator(const StatefulScriptedInterpolator&) = delete;
  StatefulScriptedInterpolator& operator=(const StatefulScriptedInterpolator&) =
      delete;

  ScriptedInterpolator* get() const { return m_clone.get(); }

  /// Points the clone at a new data context and re-runs its Lua init,
  /// mirroring what Artboard / StateMachineInstance do for their own
  /// scripted objects when they are rebound.
  void dataContext(const rcp<DataContext>& dataContext);

  /// Runs the clone's Lua init if it hasn't run yet. Same guard as
  /// Artboard::initScriptedObjects.
  void initialize();

  /// Self-contained holders only: rebind the input binds to `dataContext`
  /// and point the clone at it.
  void bindFromContext(const rcp<DataContext>& dataContext);

  /// Self-contained holders only: release the input binds' context.
  void unbind();

  /// Self-contained holders only: apply input binds that changed.
  void updateDataBinds();

private:
  void cloneFrom(
      const ScriptedInterpolator* source,
      const rcp<DataContext>& dataContext);

  std::unique_ptr<StatefulScriptedInterpolatorBinds> m_ownedContainer;
  DataBindContainer* m_container;
  std::vector<DataBind*> m_dataBinds;
  std::unique_ptr<ScriptedInterpolator> m_clone;
};
} // namespace rive

#endif