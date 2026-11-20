#ifndef _RIVE_SCRIPTED_OBJECT_HPP_
#define _RIVE_SCRIPTED_OBJECT_HPP_
#include "rive/assets/file_asset_referencer.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/custom_property.hpp"
#include "rive/custom_property_container.hpp"
#include "rive/refcnt.hpp"
#include "rive/generated/assets/script_asset_base.hpp"
#ifdef WITH_RIVE_SCRIPTING
#include "rive/scripted/script_backend.hpp"
#include "rive/scripted/scripted_read_tracker.hpp"
#endif
#include <algorithm>
#include <vector>

namespace rive
{
class Artboard;
class Component;
class DataContext;
class ViewModelInstanceValue;
class DataBindContainer;
class ScriptedProperty;
class ScriptedContext;
class TransformComponent;
#ifdef WITH_RIVE_TOOLS
class WasmNodeSource;
#endif

class ScriptedObject : public FileAssetReferencer,
                       public CustomPropertyContainer,
                       public OptionalScriptedMethods
{
protected:
  int m_self = 0;
  int m_context = 0;
  ScriptedContext* m_contextPtr = nullptr;
  virtual void disposeScriptInputs();

public:
  /// The backend announces display scale changes here; layouts re-run
  /// their resize callback so scripts see the new scale.
  virtual void displayScaleChanged() {}
  /// A promise this object's backend made settled from async work (see
  /// ScriptBackend::asyncWorkDelivered). Its continuation may have changed
  /// script state without raising any dirt, so an object that only runs on
  /// change (an idle advance, a cached conversion) runs again. Objects the
  /// runtime calls whenever their owner runs need nothing.
  virtual void asyncWorkDelivered() {}

protected:
#ifdef WITH_RIVE_SCRIPTING
  // Non-owning. The backend tracks every ScriptedObject that points at it
  // (via registerScriptedObject in ensureScriptInitialized) and nulls these
  // out at teardown. Holding it owned would create a cycle:
  // ScriptingVM → lua_State → ScriptedArtboard userdata →
  // ScriptReffedArtboard → inner ArtboardInstance → ScriptedObject →
  // rcp<ScriptingVM>.
  ScriptBackend* m_vm = nullptr;
  friend class ScriptBackend;
  friend class ScriptingVM;
  friend class WasmScriptingVM;
#endif
  bool inUpdatePhase() const { return m_inUpdatePhase; }
  void setInUpdatePhase(bool value) { m_inUpdatePhase = value; }

private:
  rcp<DataContext> m_dataContext = nullptr;
  std::vector<ScriptedProperty*> m_trackedScriptedProperties;
  bool m_inUpdatePhase = false;
#ifdef WITH_RIVE_SCRIPTING
  bool m_userLuaInitDone = false;
  // Whether the running update or advance wrote something after reading it;
  // see wroteAfterRead.
  bool m_readsWentStale = false;
  bool tryUserInit();
  // The object whose update or advance is running on this thread and
  // noting what it reads, null when none is; see trackRead.
  static thread_local ScriptedObject* s_reader;
  // What the running update or advance read, kept between runs for the
  // capacity.
  std::vector<ScriptedRead> m_reads;
  // The artboard tracking them.
  Artboard* m_readsArtboard = nullptr;
  // The artboard whose components its reads are tracked on, null when it
  // tracks none.
  Artboard* readsArtboard();
  void trackReads(Artboard* artboard, ScriptedReadPhase phase, uint64_t serial);
  void untrackReads();
#endif
  void disposeScriptedContext();
  void disposeTrackedProperties();

public:
  virtual ~ScriptedObject() { scriptDispose(); }
  ScriptAsset* scriptAsset() const;
  void setArtboardInput(std::string name, Artboard* artboard);
  void setBooleanInput(std::string name, bool value);
  void setIntegerInput(std::string name, int value);
  void setNumberInput(std::string name, float value);
  void setStringInput(std::string name, std::string value);
  void setViewModelInput(std::string name, ViewModelInstanceValue* value);
  /// id is a ScriptInputComponent's NodeRef id: its target's index in this
  /// object's artboard plus one, 0 for none.
  void setNodeRefInput(std::string name, uint32_t id);
  /// The node a NodeRef id names in this object's artboard, null when the
  /// object is no component, the id names nothing, or the target is no
  /// transform.
  TransformComponent* nodeRefTarget(uint32_t id) const;
  void trigger(std::string name);
  /// Runs the script's advance and update. On an object that tracksReads(),
  /// what they read from its own artboard, through trackRead, runs them
  /// again when that changes (see ScriptedReadTracker).
  bool scriptAdvance(float elapsedSeconds);
  void scriptUpdate();
  /// Whether its update and advance are run again when what they read
  /// changes; the objects whose update and advance are the script's own.
  virtual bool tracksReads() { return false; }
  /// What its last advance read changed. It runs again on the next frame,
  /// and draws after it whatever it returns, as the script cannot tell that
  /// advance from one it asked for.
  virtual void readsWokeAdvance() {}
#ifdef WITH_RIVE_SCRIPTING
  /// Notes that the update or advance running now read `component`, which
  /// goes stale with `dirt`. It counts for the object running, whichever
  /// object's node it read through, as a script can keep a node another
  /// one handed it. Reads in draw are not tracked: it runs every frame the
  /// artboard draws, after the update pass, so it reads current values.
  static void trackRead(Component* component, ComponentDirt dirt)
  {
    if (s_reader != nullptr)
    {
      s_reader->m_reads.push_back({component, dirt});
    }
  }
  /// Notes that a script wrote `component` in a way that raises no dirt,
  /// as a world transform write does. Returns whether the update or advance
  /// running had already read it, going stale with `dirt`: the write left
  /// that read stale, so it stays due once it finishes rather than taking
  /// the write as seen.
  static bool wroteAfterRead(Component* component, ComponentDirt dirt);
#endif
  void reinit();
#ifdef WITH_RIVE_SCRIPTING
  bool userLuaInitDone() { return m_userLuaInitDone; }
  void resetLuaInit() { m_userLuaInitDone = false; }
#else
  bool userLuaInitDone() { return true; }
  void resetLuaInit() {}
#endif
  virtual void markNeedsUpdate();
  virtual rcp<DataContext> dataContext() { return m_dataContext; }
  void dataContext(rcp<DataContext> value) { m_dataContext = value; }
#ifdef WITH_RIVE_SCRIPTING
  /// Run the generator into m_self once per backend; does not push inputs
  /// or call init(); use hydrateScriptInputs() after data bind.
  bool ensureScriptInitialized(ScriptBackend* vm, int generatorRef);
  /// Resolve inputs from DataContext and push into the script; runs init()
  /// once after first successful hydration when implemented.
  bool hydrateScriptInputs();
  ScriptBackend* backend() const { return m_vm; }
#else
  bool hydrateScriptInputs() { return true; }
#endif
  void scriptDispose();
  virtual bool addScriptedDirt(ComponentDirt value, bool recurse = false) = 0;
  void setAsset(rcp<FileAsset> asset) override;
  static ScriptedObject* from(Core* object);
  virtual ScriptProtocol scriptProtocol() = 0;
  int self() { return m_self; }
  virtual Component* component() = 0;
#ifdef WITH_RIVE_TOOLS
  /// The nodes of an object whose artboard is not a runtime one, like the
  /// editor's Dart stage, where component() is null: a source, and its ids
  /// for the object's artboard and for the object, 0 when it is not a node.
  struct StageNodes
  {
    WasmNodeSource* source = nullptr;
    uint32_t artboard = 0;
    uint32_t self = 0;
  };
  virtual StageNodes stageNodes() { return {}; }
#endif
  virtual ScriptedObject* cloneScriptedObject(DataBindContainer*) const
  {
    return nullptr;
  }
  void cloneProperties(CustomPropertyContainer*, DataBindContainer*) const;
  // Defined with ScriptedProperty (Luau backend only).
  void addTrackedScriptedProperty(ScriptedProperty* property);
  void removeTrackedScriptedProperty(ScriptedProperty* property);
  const std::vector<ScriptedProperty*>& trackedScriptedProperties() const
  {
    return m_trackedScriptedProperties;
  }
  virtual bool addDataBindFromScriptedObject(DataBind*) { return false; }
#ifdef WITH_RIVE_SCRIPTING
  /// Called after hydrateScriptInputs() succeeds;
  virtual void didHydrateScriptInputs() {}
  // A reinit replaced script state, like a VM swap on editor pause; hosts
  // re-record content the old state produced.
  virtual void didReinit() {}
#endif
};
} // namespace rive

#endif