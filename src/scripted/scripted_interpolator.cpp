#include "rive/component_dirt.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_bind_container.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/scripted/scripted_interpolator.hpp"

using namespace rive;

#ifdef WITH_RIVE_SCRIPTING

// Mirrors the Dart `_interpolatorBoilerplate` defaults: identity for
// `transform`, linear interpolation for `transformValue`. Used whenever the
// Lua side isn't ready (no VM, no `m_self`), the script doesn't define the
// method, or the pcall fails — so the runtime degrades to standard linear
// behavior instead of producing garbage.
static inline float defaultTransformValue(float from, float to, float factor)
{
  return from + (to - from) * factor;
}

float ScriptedInterpolator::transform(float factor) const
{
  if (m_vm == nullptr || !m_vm->valid() || m_self == 0)
  {
    return factor;
  }
  float result = factor;
  // callNumberMethod wants a non-const ScriptedObject*; transform() is
  // const because it doesn't mutate the C++ object — only the script side.
  m_vm->callNumberMethod(
      const_cast<ScriptedInterpolator*>(this),
      m_self,
      "transform",
      &factor,
      1,
      &result);
  return result;
}

float ScriptedInterpolator::transformValue(
    float valueFrom,
    float valueTo,
    float factor)
{
  if (m_vm == nullptr || !m_vm->valid() || m_self == 0)
  {
    return defaultTransformValue(valueFrom, valueTo, factor);
  }
  float args[3] = {valueFrom, valueTo, factor};
  float result = 0.0f;
  if (!m_vm->callNumberMethod(this, m_self, "transformValue", args, 3, &result))
  {
    return defaultTransformValue(valueFrom, valueTo, factor);
  }
  return result;
}

#else

float ScriptedInterpolator::transform(float factor) const { return factor; }

float ScriptedInterpolator::transformValue(
    float valueFrom,
    float valueTo,
    float factor)
{
  return valueFrom + (valueTo - valueFrom) * factor;
}

#endif

void ScriptedInterpolator::addProperty(CustomProperty* prop)
{
  auto* scriptInput = ScriptInput::from(prop);
  if (scriptInput != nullptr)
  {
    scriptInput->scriptedObject(this);
  }
  CustomPropertyContainer::addProperty(prop);
}

StatusCode ScriptedInterpolator::import(ImportStack& importStack)
{
  auto result = registerReferencer(importStack);
  if (result != StatusCode::Ok)
  {
    return result;
  }
  return Super::import(importStack);
}

// Mirrors ScriptedListenerAction::clone — preserve the file asset reference
// across the clone so the new instance vends the same script bytecode.
Core* ScriptedInterpolator::clone() const
{
  auto* twin = ScriptedInterpolatorBase::clone()->as<ScriptedInterpolator>();
  if (m_fileAsset != nullptr)
  {
    twin->setAsset(m_fileAsset);
  }
  return twin;
}

ScriptedInterpolator::~ScriptedInterpolator() { disposeScriptInputs(); }

// Mirrors ScriptedDataConverter / ScriptedListenerAction /
// ScriptedTransitionCondition: a ScriptedObject that is not a Component is not
// an artboard child, so ScriptInput*::import never routes its inputs to the
// ArtboardImporter and Artboard::m_Objects never frees them.
void ScriptedInterpolator::disposeScriptInputs()
{
  auto props = m_customProperties;
  ScriptedObject::disposeScriptInputs();
  for (auto prop : props)
  {
    auto scriptInput = ScriptInput::from(prop);
    if (scriptInput != nullptr)
    {
#ifdef WITH_RIVE_EDITOR
      if (prop->editorArena() != nullptr)
        continue;
#endif
      delete scriptInput;
    }
  }
}

// Used by LinearAnimationInstance::statefulInterpolator() to vend a per-(LAI,
// keyframe) Lua instance lazily. `dataBindContainer` is the
// ArtboardInstance — it owns any cloned data binds.
ScriptedObject* ScriptedInterpolator::cloneScriptedObject(
    DataBindContainer* dataBindContainer) const
{
  auto* cloned = clone()->as<ScriptedInterpolator>();
  cloneProperties(cloned, dataBindContainer);
  cloned->reinit();
  return cloned;
}

namespace rive
{
// Exposes the bind plumbing a self-contained StatefulScriptedInterpolator
// forwards from its owner.
class StatefulScriptedInterpolatorBinds final : public DataBindContainer
{
public:
  void bind(const rcp<DataContext>& dataContext)
  {
    bindDataBindsFromContext(dataContext);
  }
  void unbind() { unbindDataBinds(); }
};
} // namespace rive

StatefulScriptedInterpolator::StatefulScriptedInterpolator(
    const ScriptedInterpolator* source,
    DataBindContainer* container,
    const rcp<DataContext>& dataContext)
    : m_container(container)
{
  cloneFrom(source, dataContext);
}

StatefulScriptedInterpolator::StatefulScriptedInterpolator(
    const ScriptedInterpolator* source,
    const rcp<DataContext>& dataContext)
    : m_ownedContainer(new StatefulScriptedInterpolatorBinds())
    , m_container(m_ownedContainer.get())
{
  cloneFrom(source, dataContext);
}

void StatefulScriptedInterpolator::cloneFrom(
    const ScriptedInterpolator* source,
    const rcp<DataContext>& dataContext)
{
  if (source == nullptr || m_container == nullptr)
  {
    return;
  }
  auto* cloned = source->cloneScriptedObject(m_container);
  if (cloned == nullptr)
  {
    return;
  }
  m_clone.reset(static_cast<ScriptedInterpolator*>(cloned));
  m_clone->dataContext(dataContext);

  // cloneProperties() just parked a bind on `container` for every bound
  // input, and wired the cloned input's back-pointer to it. Record them so
  // the destructor can pull them out before the clone (their target) dies.
  for (auto* prop : m_clone->customProperties())
  {
    if (auto* input = ScriptInput::from(prop))
    {
      if (auto* bind = input->dataBind())
      {
        m_dataBinds.push_back(bind);
      }
    }
  }

  // Late-binding safety net: reinit() inside cloneScriptedObject is a no-op
  // when scriptAsset() is null at clone time. If the asset became available
  // later (editor live-edit), init now.
  if (m_clone->scriptAsset() != nullptr && !m_clone->userLuaInitDone())
  {
    m_clone->scriptAsset()->initScriptedObject(m_clone.get());
    m_clone->hydrateScriptInputs();
  }
}

StatefulScriptedInterpolator::~StatefulScriptedInterpolator()
{
  for (auto* bind : m_dataBinds)
  {
    m_container->removeDataBind(bind);
    delete bind;
  }
}

void StatefulScriptedInterpolator::dataContext(
    const rcp<DataContext>& dataContext)
{
  if (m_clone == nullptr)
  {
    return;
  }
  m_clone->dataContext(dataContext);
  initialize();
}

void StatefulScriptedInterpolator::bindFromContext(
    const rcp<DataContext>& dataContext)
{
  if (m_ownedContainer == nullptr || m_clone == nullptr)
  {
    return;
  }
  m_ownedContainer->bind(dataContext);
  // Binding only marks the input binds dirty; apply them now so the inputs
  // hold this context's values before init() reads them.
  m_ownedContainer->updateDataBinds(false);
  // The clone first ran init() in cloneFrom, before its inputs had a context
  // to resolve against. Run it again now that they do, and on every rebind,
  // as ScriptedDataConverter does in its own bindFromContext.
  m_clone->resetLuaInit();
  this->dataContext(dataContext);
}

void StatefulScriptedInterpolator::unbind()
{
  if (m_ownedContainer != nullptr)
  {
    m_ownedContainer->unbind();
  }
}

void StatefulScriptedInterpolator::updateDataBinds()
{
  if (m_ownedContainer != nullptr)
  {
    m_ownedContainer->updateDataBinds(false);
  }
}

void StatefulScriptedInterpolator::initialize()
{
  if (m_clone == nullptr || m_clone->scriptAsset() == nullptr)
  {
    return;
  }
  if (!m_clone->userLuaInitDone())
  {
    m_clone->scriptAsset()->initScriptedObject(m_clone.get());
  }
  m_clone->hydrateScriptInputs();
}

#ifdef WITH_RIVE_EDITOR
void ScriptedInterpolator::editorPrepareForDestruction()
{
  scriptDispose();
  dataContext(nullptr);
}

#endif
