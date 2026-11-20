#ifdef WITH_RIVE_SCRIPTING
#ifdef WITH_RIVE_SCRIPTING_LUAU
#include "rive/lua/rive_lua_libs.hpp"
#endif
#endif
#include "rive/assets/script_asset.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/script_input_artboard.hpp"
#include "rive/animation/scripted_listener_action.hpp"
#include "rive/animation/scripted_transition_condition.hpp"
#include "rive/scripted/scripted_data_converter.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/scripted/scripted_interpolator.hpp"
#include "rive/scripted/scripted_layout.hpp"
#include "rive/scripted/scripted_transition.hpp"
#include "rive/scripted/scripted_path_effect.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/transform_component.hpp"

using namespace rive;

ScriptedObject* ScriptedObject::from(Core* object)
{
  switch (object->coreType())
  {
    case ScriptedDataConverter::typeKey:
      return object->as<ScriptedDataConverter>();
    case ScriptedDrawable::typeKey:
      return object->as<ScriptedDrawable>();
    case ScriptedLayout::typeKey:
      return object->as<ScriptedLayout>();
    case ScriptedTransition::typeKey:
      return object->as<ScriptedTransition>();
    case ScriptedPathEffect::typeKey:
      return object->as<ScriptedPathEffect>();
    case ScriptedListenerAction::typeKey:
      return object->as<ScriptedListenerAction>();
    case ScriptedTransitionCondition::typeKey:
      return object->as<ScriptedTransitionCondition>();
    case ScriptedInterpolator::typeKey:
      return object->as<ScriptedInterpolator>();
  }
  return nullptr;
}

#ifdef WITH_RIVE_SCRIPTING
void ScriptedObject::setArtboardInput(std::string name, Artboard* artboard)
{
  if (m_vm == nullptr || !m_vm->valid() || scriptAsset() == nullptr)
  {
    return;
  }
  // The input is an instance of the artboard, and initializing it sets the
  // Artboard inputs of its own scripts, nested ones included. An artboard
  // that leads back to itself that way would recurse until the stack
  // overflows, so leave the input unset where it repeats.
  static thread_local std::vector<const Artboard*> instancing;
  const Artboard* source = artboard->artboardSource();
  if (std::find(instancing.begin(), instancing.end(), source) !=
      instancing.end())
  {
    return;
  }
  // Popped on unwind too: Luau raises errors as exceptions.
  struct Instancing
  {
    Instancing(const Artboard* source) { instancing.push_back(source); }
    ~Instancing() { instancing.pop_back(); }
  } scope(source);
  m_vm->setInputArtboard(m_self, name.c_str(), this, artboard);
  addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setNodeRefInput(std::string name, uint32_t id)
{
  if (m_vm == nullptr || !m_vm->valid())
  {
    return;
  }
  m_vm->setInputNodeRef(m_self, name.c_str(), this, id);
  addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setBooleanInput(std::string name, bool value)
{
  if (m_vm == nullptr || !m_vm->valid())
  {
    return;
  }
  m_vm->setInputBoolean(m_self, name.c_str(), this, value);
  addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setNumberInput(std::string name, float value)
{
  if (m_vm == nullptr || !m_vm->valid())
  {
    return;
  }
  m_vm->setInputNumber(m_self, name.c_str(), this, value);
  addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setIntegerInput(std::string name, int value)
{
  if (m_vm == nullptr || !m_vm->valid())
  {
    return;
  }
  m_vm->setInputUnsigned(m_self, name.c_str(), this, value);
  addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setStringInput(std::string name, std::string value)
{
  if (m_vm == nullptr || !m_vm->valid())
  {
    return;
  }
  m_vm->setInputString(m_self, name.c_str(), this, value.c_str());
  addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setViewModelInput(
    std::string name,
    ViewModelInstanceValue* value)
{
  if (m_vm == nullptr || !m_vm->valid())
  {
    return;
  }
  m_vm->setInputViewModel(m_self, name.c_str(), this, value);
  addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::trigger(std::string name)
{
  if (m_vm == nullptr || !m_vm->valid())
  {
    return;
  }
  m_vm->callTrigger(this, m_self, name.c_str());
  addScriptedDirt(ComponentDirt::ScriptUpdate);
}

thread_local ScriptedObject* ScriptedObject::s_reader = nullptr;

bool ScriptedObject::scriptAdvance(float elapsedSeconds)
{
  if (!advances() || m_vm == nullptr || !m_vm->valid())
  {
    return false;
  }
  // The last component update before it: what it reads is current up to
  // that one.
  auto artboard = readsArtboard();
  uint64_t serial = artboard != nullptr ? artboard->updateSerial() : 0;
  m_reads.clear();
  m_readsWentStale = false;
  auto reader = s_reader;
  s_reader = artboard != nullptr ? this : nullptr;
  bool result = m_vm->callAdvance(this, m_self, elapsedSeconds);
  s_reader = reader;
  if (artboard != nullptr)
  {
    trackReads(artboard, ScriptedReadPhase::advance, serial);
  }
  return result;
}

void ScriptedObject::scriptUpdate()
{
  if (!updates() || m_vm == nullptr || !m_vm->valid())
  {
    return;
  }
  // The update running now, the object's own: what it reads is current up
  // to it.
  auto artboard = readsArtboard();
  uint64_t serial = artboard != nullptr ? artboard->updateSerial() : 0;
  m_reads.clear();
  m_readsWentStale = false;
  auto reader = s_reader;
  s_reader = artboard != nullptr ? this : nullptr;
  m_vm->callUpdate(this, m_self);
  s_reader = reader;
  if (artboard != nullptr)
  {
    trackReads(artboard, ScriptedReadPhase::update, serial);
  }
}

Artboard* ScriptedObject::readsArtboard()
{
  if (!tracksReads())
  {
    return nullptr;
  }
  auto owner = component();
  return owner != nullptr ? owner->artboard() : nullptr;
}

void ScriptedObject::trackReads(
    Artboard* artboard,
    ScriptedReadPhase phase,
    uint64_t serial)
{
  if (m_readsArtboard != artboard)
  {
    untrackReads();
  }
  // Only the artboard's own components update with it.
  m_reads.erase(
      std::remove_if(
          m_reads.begin(),
          m_reads.end(),
          [artboard](const ScriptedRead& read) {
            return read.component->artboard() != artboard;
          }),
      m_reads.end());
  // Nothing read now and nothing read before: no tracker needed.
  if (m_reads.empty() && m_readsArtboard == nullptr)
  {
    return;
  }
  bool tracked = artboard->makeReadTracker()
                     ->track(this, phase, serial, m_reads, m_readsWentStale);
  m_readsArtboard = tracked ? artboard : nullptr;
}

bool ScriptedObject::wroteAfterRead(Component* component, ComponentDirt dirt)
{
  auto reader = s_reader;
  // Reads of other artboards are not tracked.
  if (reader == nullptr || component->artboard() != reader->readsArtboard())
  {
    return false;
  }
  for (auto& read : reader->m_reads)
  {
    if (read.component == component &&
        (read.dirt & dirt) != ComponentDirt::None)
    {
      reader->m_readsWentStale = true;
      return true;
    }
  }
  return false;
}

void ScriptedObject::untrackReads()
{
  if (m_readsArtboard == nullptr)
  {
    return;
  }
  if (auto tracker = m_readsArtboard->readTracker())
  {
    tracker->untrack(this);
  }
  m_readsArtboard = nullptr;
}

bool ScriptedObject::tryUserInit()
{
  switch (m_vm->callUserInit(this, m_self, m_context))
  {
    case ScriptBackend::InitResult::notImplemented:
    case ScriptBackend::InitResult::succeeded:
      return true;
    case ScriptBackend::InitResult::failed:
      break;
  }
  m_vm->releaseRef(m_self);
  disposeScriptedContext();
  m_vm->unregisterScriptedObject(this);
  m_vm = nullptr;
  m_self = 0;
  return false;
}

bool ScriptedObject::ensureScriptInitialized(
    ScriptBackend* vm,
    int generatorRef)
{
  if (vm == nullptr || !vm->valid())
  {
    return false;
  }
  if (m_self != 0 && m_vm == vm)
  {
    return true;
  }
  if (m_vm != nullptr)
  {
    if (m_self != 0)
    {
      m_vm->releaseRef(m_self);
      m_self = 0;
    }

    disposeTrackedProperties();
    disposeScriptedContext();
    m_vm->unregisterScriptedObject(this);
  }
  m_userLuaInitDone = false;

  for (auto prop : m_customProperties)
  {
    auto scriptInput = ScriptInput::from(prop);
    if (scriptInput && !scriptInput->validateForColdScriptInit())
    {
      return false;
    }
  }

  // m_vm is assigned only after a successful instantiate, so the failure
  // path leaves the object fully detached.
  m_self = vm->instantiate(generatorRef, this, &m_context, &m_contextPtr);
  if (m_self == 0)
  {
    m_context = 0;
    m_contextPtr = nullptr;
    return false;
  }
  m_vm = vm;
  vm->registerScriptedObject(this);
  return true;
}

bool ScriptedObject::hydrateScriptInputs()
{
  if (m_vm == nullptr || !m_vm->valid() || m_self == 0)
  {
    return false;
  }
  // First validate that all properties are hydratable.
  // This ensures we don't partially hydrate props and early out on the next
  // step
  for (auto prop : m_customProperties)
  {
    auto scriptInput = ScriptInput::from(prop);
    if (scriptInput != nullptr &&
        !scriptInput->validateHydrationPrerequisites())
    {
      return false;
    }
  }
  // Next hydrate all properties
  for (auto prop : m_customProperties)
  {
    auto scriptInput = ScriptInput::from(prop);
    if (scriptInput != nullptr && !scriptInput->hydrateScriptInput())
    {
      return false;
    }
  }
  // Finally initialize the script if it hasn't been initialized before.
  if (inits() && !m_userLuaInitDone)
  {
    if (!tryUserInit())
    {
      return false;
    }
    m_userLuaInitDone = true;
  }
  didHydrateScriptInputs();
  return true;
}

void ScriptedObject::disposeTrackedProperties()
{
// Tracked properties only exist on the Luau backend; the wasm module keeps
// its property wrappers module side.
#ifdef WITH_RIVE_SCRIPTING_LUAU
  auto trackedProperties = m_trackedScriptedProperties;
  for (auto* property : trackedProperties)
  {
    if (property != nullptr)
    {
      property->dispose();
    }
  }
#endif
}

void ScriptedObject::disposeScriptedContext()
{
#ifdef WITH_RIVE_SCRIPTING_LUAU
  if (m_contextPtr != nullptr)
  {
    m_contextPtr->clearScriptedObject();
    m_contextPtr = nullptr;
  }
#endif
  if (m_context != 0)
  {
    if (m_vm != nullptr && m_vm->valid())
    {
      m_vm->releaseRef(m_context);
    }
    m_context = 0;
  }
}

void ScriptedObject::scriptDispose()
{
  disposeScriptInputs();
  disposeTrackedProperties();
  m_trackedScriptedProperties.clear();
  // Through the artboard it kept, not component(): this also runs from the
  // destructor, where that is no longer callable.
  untrackReads();

  if (m_vm != nullptr && m_vm->valid())
  {
    m_vm->releaseRef(m_self);
    disposeScriptedContext();
  }
  if (m_vm != nullptr)
  {
    m_vm->unregisterScriptedObject(this);
    m_vm = nullptr;
  }
  m_self = 0;
  m_userLuaInitDone = false;
}
#else
void ScriptedObject::setArtboardInput(std::string name, Artboard* artboard) {}

void ScriptedObject::setBooleanInput(std::string name, bool value) {}

void ScriptedObject::setNodeRefInput(std::string name, uint32_t id) {}

void ScriptedObject::setIntegerInput(std::string name, int value) {}

void ScriptedObject::setNumberInput(std::string name, float value) {}

void ScriptedObject::setStringInput(std::string name, std::string value) {}

void ScriptedObject::setViewModelInput(
    std::string name,
    ViewModelInstanceValue* value)
{}

void ScriptedObject::trigger(std::string name) {}

bool ScriptedObject::scriptAdvance(float elapsedSeconds) { return false; }

void ScriptedObject::scriptUpdate() {}

// Inputs hold a back-pointer to their scripted object; without disposal
// they dangle at teardown even when scripting is compiled out.
void ScriptedObject::scriptDispose() { disposeScriptInputs(); }
#endif

// Shared by both scripting flavors.
TransformComponent* ScriptedObject::nodeRefTarget(uint32_t id) const
{
  auto self = const_cast<ScriptedObject*>(this)->component();
  if (id == 0 || self == nullptr || self->artboard() == nullptr)
  {
    return nullptr;
  }
  // The artboard counts as a Node, but a NodeRef names a node in it: the
  // editor places an artboard where the runtime zeroes its position.
  auto artboard = self->artboard();
  auto target = artboard->resolve(id - 1);
  if (target == nullptr || target == artboard ||
      !target->is<TransformComponent>())
  {
    return nullptr;
  }
  return target->as<TransformComponent>();
}

void ScriptedObject::disposeScriptInputs()
{
  for (auto prop : m_customProperties)
  {
    auto scriptInput = ScriptInput::from(prop);
    if (scriptInput != nullptr)
    {
      scriptInput->scriptedObject(nullptr);
    }
  }
  m_customProperties.clear();
}

void ScriptedObject::reinit()
{
  if (scriptAsset() != nullptr)
  {
    scriptAsset()->initScriptedObject(this);
#ifdef WITH_RIVE_SCRIPTING
    hydrateScriptInputs();
    didReinit();
#endif
  }
}

ScriptAsset* ScriptedObject::scriptAsset() const
{
  return (ScriptAsset*)m_fileAsset.get();
}

void ScriptedObject::setAsset(rcp<FileAsset> asset)
{
  if (asset != nullptr && asset->is<ScriptAsset>())
  {
    FileAssetReferencer::setAsset(asset);
  }
}

void ScriptedObject::markNeedsUpdate() {}

void ScriptedObject::cloneProperties(
    CustomPropertyContainer* twin,
    DataBindContainer* dataBindContainer) const
{

  for (auto prop : m_customProperties)
  {
    auto clonedValue = prop->clone()->as<CustomProperty>();
    twin->addProperty(clonedValue);
    auto input = ScriptInput::from(prop);
    if (input != nullptr)
    {
      auto dataBind = input->dataBind();
      if (dataBind)
      {
        auto dataBindClone = static_cast<DataBind*>(dataBind->clone());
        dataBindClone->file(dataBind->file());
        if (dataBind->converter() != nullptr)
        {
          dataBindClone->converter(
              dataBind->converter()->clone()->as<DataConverter>());
        }
        dataBindClone->target(clonedValue);
        dataBindContainer->addDataBind(dataBindClone);
        if (auto* clonedInput = ScriptInput::from(clonedValue))
        {
          clonedInput->dataBind(
              dataBindClone,
              /*ownsDataBind=*/false);
        }
      }
    }
  }
}