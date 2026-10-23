#include "rive/text/text_style_share_cache.hpp"
#include "rive/animation/any_state.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/animation/exit_state.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/artboard.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/data_bind/converters/data_converter_group.hpp"
#include "rive/data_bind/converters/data_converter_group_item.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include "rive/file.hpp"
#include "rive/generated/data_bind/converters/data_converter_formula_base.hpp"
#include "rive/generated/data_bind/converters/data_converter_operation_viewmodel_base.hpp"
#include "rive/generated/scripted/scripted_data_converter_base.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/gradient_stop.hpp"
#include "rive/shapes/paint/linear_gradient.hpp"
#include "rive/shapes/paint/radial_gradient.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/text/text_interface.hpp"
#include "rive/text/text_style_axis.hpp"
#include "rive/text/text_style_feature.hpp"
#include "rive/text/text_style_global.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include <algorithm>

using namespace rive;

namespace
{
// Only plain paints can be drawn by several Texts: the glyph outlines each
// Text draws are its own (see TextStyleGeometry), but backgrounds keep their
// Text's glyph rects, path effects and inner feathers are built from the
// paint's own glyph path, and an image in a paint fits that Text's bounds.
// Anything else (scripted objects, nested artboards...) may animate. Unknown
// types are refused, so a new type can never silently break sharing.
bool isPlainStyleObject(const Core* object)
{
  switch (object->coreType())
  {
    case ArtboardBase::typeKey:
    case LayoutComponentStyleBase::typeKey:
    case TextStylePaintBase::typeKey:
    case TextStyleAxisBase::typeKey:
    case TextStyleFeatureBase::typeKey:
    case FillBase::typeKey:
    case StrokeBase::typeKey:
    case SolidColorBase::typeKey:
    case LinearGradientBase::typeKey:
    case RadialGradientBase::typeKey:
    case GradientStopBase::typeKey:
    // Binds are checked on their own (see analyze).
    case DataBindBase::typeKey:
    case DataBindContextBase::typeKey:
      return true;
    case FeatherBase::typeKey:
      return !object->as<Feather>()->inner();
    default:
      return false;
  }
}

bool isInertState(const LayerState* state)
{
  return state->is<EntryState>() || state->is<AnyState>() ||
         state->is<ExitState>();
}

// Only the default state machine ever runs on a style. It does nothing when
// it has no listeners, no binds, and no state beyond Entry/Any/Exit.
bool hasStateMachineLogic(const Artboard* source)
{
  int index = source->defaultStateMachineIndex();
  if (index < 0)
  {
    return false;
  }
  const StateMachine* machine = source->stateMachine((size_t)index);
  if (machine == nullptr)
  {
    return false;
  }
  if (machine->listenerCount() != 0 || machine->dataBindCount() != 0)
  {
    return true;
  }
  for (size_t i = 0; i < machine->layerCount(); i++)
  {
    const StateMachineLayer* layer = machine->layer(i);
    for (size_t j = 0; j < layer->stateCount(); j++)
    {
      if (!isInertState(layer->state(j)))
      {
        return true;
      }
    }
  }
  return false;
}

// Pointer listeners with no target listen to the run the style is drawn for.
bool listensToRuns(const Artboard* source)
{
  int index = source->defaultStateMachineIndex();
  if (index < 0)
  {
    return false;
  }
  const StateMachine* machine = source->stateMachine((size_t)index);
  if (machine == nullptr)
  {
    return false;
  }
  for (size_t i = 0; i < machine->listenerCount(); i++)
  {
    const StateMachineListener* listener = machine->listener(i);
    if (listener != nullptr && listener->hasPointerListeners() &&
        source->resolve(listener->targetId()) == nullptr)
    {
      return true;
    }
  }
  return false;
}

// A converter that never changes on its own and reads nothing but its input.
bool isStaticConverter(DataConverter* converter)
{
  if (converter == nullptr)
  {
    return true;
  }
  switch (converter->coreType())
  {
    case DataConverterFormulaBase::typeKey:
    case DataConverterOperationViewModelBase::typeKey:
    case ScriptedDataConverterBase::typeKey:
      return false;
    default:
      break;
  }
  if (converter->mayAdvance() || !converter->dataBinds().empty())
  {
    return false;
  }
  if (converter->is<DataConverterGroup>())
  {
    for (auto item : converter->as<DataConverterGroup>()->items())
    {
      if (!isStaticConverter(item->converter()))
      {
        return false;
      }
    }
  }
  return true;
}
} // namespace

TextStyleShareInfo TextStyleShareCache::analyze(
    const Artboard* source,
    const File* file)
{
  TextStyleShareInfo info;
  if (source == nullptr)
  {
    return info;
  }
  uint32_t ownViewModelId = source->viewModelId();
  // Whether [dataBind] reads nothing but the style's own view model.
  auto readsOwnViewModel = [ownViewModelId](const DataBind* dataBind) {
    if (!dataBind->is<DataBindContext>() || dataBind->isNameBased())
    {
      return false;
    }
    auto& path = dataBind->as<DataBindContext>()->sourcePathIds();
    return !path.empty() && path[0] == ownViewModelId;
  };
  bool onlyOwnViewModel = true;
  bool staticBinds = true;
  for (auto dataBind : source->dataBinds())
  {
    if (!readsOwnViewModel(dataBind))
    {
      onlyOwnViewModel = false;
    }
    if (dataBind->is<DataBindContext>() && !dataBind->isNameBased() &&
        !isStaticConverter(dataBind->converter()))
    {
      staticBinds = false;
    }
  }
  // The state machine's binds (its conditions, listener actions) read the
  // same data context.
  int machineIndex = source->defaultStateMachineIndex();
  const StateMachine* machine = machineIndex >= 0
                                    ? source->stateMachine((size_t)machineIndex)
                                    : nullptr;
  if (machine != nullptr)
  {
    for (size_t i = 0; i < machine->dataBindCount(); i++)
    {
      auto dataBind = machine->dataBind(i);
      if (dataBind != nullptr && !readsOwnViewModel(dataBind))
      {
        onlyOwnViewModel = false;
      }
    }
  }
  info.readsOutsideOwnViewModel = !onlyOwnViewModel;
  info.listensToRuns = listensToRuns(source);
  info.hasStateMachine = hasStateMachineLogic(source);

#ifdef WITH_RIVE_EDITOR
  // The editor edits source artboards in place; never freeze one.
  return info;
#else
  if (!onlyOwnViewModel || !staticBinds || info.hasStateMachine)
  {
    return info;
  }
  for (auto object : source->objects())
  {
    if (object != nullptr && !isPlainStyleObject(object))
    {
      return info;
    }
  }
  // Exposed properties take per-text values.
  ViewModel* viewModel = file != nullptr ? file->viewModel(ownViewModelId)
                                         : nullptr;
  if (viewModel != nullptr)
  {
    for (auto property : viewModel->properties())
    {
      if (property->isInput() || property->isOutput())
      {
        return info;
      }
    }
  }
  info.shareable = true;
  return info;
#endif
}

TextStyleShareInfo TextStyleShareCache::info(const Artboard* source)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  auto itr = m_info.find(source);
  if (itr != m_info.end())
  {
    return itr->second;
  }
  auto info = analyze(source, m_file);
  m_info[source] = info;
  return info;
}

bool TextStyleShareCache::anyListensToRuns()
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_anyListensToRuns != -1)
    {
      return m_anyListensToRuns == 1;
    }
  }
  bool any = false;
  if (m_file != nullptr)
  {
    for (auto artboard : m_file->artboards())
    {
      if (TextStyleGlobal::firstTextStylePaint(artboard) != nullptr &&
          info(artboard).listensToRuns)
      {
        any = true;
        break;
      }
    }
  }
  std::lock_guard<std::mutex> lock(m_mutex);
  m_anyListensToRuns = any ? 1 : 0;
  return any;
}

void TextStyleShareCache::forget(const Artboard* source)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_info.erase(source);
  m_anyListensToRuns = -1;
}

SharedTextStyleLease TextStyleShareCache::acquire(
    const Artboard* source,
    Factory* factory,
    TextInterface* consumer)
{
  // The consumers are the lease count.
  if (consumer == nullptr || !info(source).shareable)
  {
    return SharedTextStyleLease();
  }
  std::lock_guard<std::mutex> lock(m_mutex);
  auto key = std::make_pair(source, factory);
  auto itr = m_shared.find(key);
  SharedTextStyle* style = nullptr;
  if (itr != m_shared.end())
  {
    style = itr->second.get();
  }
  else
  {
    auto created = std::make_unique<SharedTextStyle>(source, m_file, factory);
    if (created->paint() == nullptr)
    {
      return SharedTextStyleLease();
    }
    style = created.get();
    m_shared[key] = std::move(created);
  }
  style->m_consumers.push_back(consumer);
  return SharedTextStyleLease(m_link, style, consumer);
}

void TextStyleShareCache::release(
    SharedTextStyle* style,
    TextInterface* consumer)
{
  std::unique_ptr<SharedTextStyle> dead;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& consumers = style->m_consumers;
    auto found = std::find(consumers.begin(), consumers.end(), consumer);
    if (found != consumers.end())
    {
      consumers.erase(found);
    }
    if (!consumers.empty())
    {
      return;
    }
    for (auto itr = m_shared.begin(); itr != m_shared.end(); ++itr)
    {
      if (itr->second.get() == style)
      {
        dead = std::move(itr->second);
        m_shared.erase(itr);
        break;
      }
    }
  }
  // Destroyed outside the lock.
}

TextStyleShareCache::~TextStyleShareCache()
{
  // Leases can outlive the cache (see TextStyleShareLink): cut them loose,
  // and free the shared instances now, while the File still has the style
  // artboards they were made from.
  m_link->cache = nullptr;
  m_shared.clear();
}

SharedTextStyle::SharedTextStyle(
    const Artboard* source,
    File* file,
    Factory* factory)
{
  m_instance = source->instance<ArtboardInstance>(factory);
  m_paint = TextStyleGlobal::firstTextStylePaint(m_instance.get());
  ViewModel* viewModel = file != nullptr
                             ? file->viewModel(source->viewModelId())
                             : nullptr;
  if (viewModel != nullptr)
  {
    m_viewModelInstance = file->createDefaultViewModelInstance(viewModel);
    if (m_viewModelInstance != nullptr)
    {
      file->completeViewModelProperties(m_viewModelInstance.get());
      m_instance->bindViewModelInstance(m_viewModelInstance, nullptr);
    }
  }
  // Its values never change after this.
  m_instance->updateDataBinds();
  m_instance->updatePass(false);
  if (m_paint != nullptr)
  {
    m_paint->owner(this);
    m_paint->isShared(true);
  }
}

SharedTextStyle::~SharedTextStyle()
{
  // The instance's data binds reference the view model instance.
  m_instance = nullptr;
  m_viewModelInstance = nullptr;
}

void SharedTextStyle::styleShapeChanged()
{
  m_instance->updatePass(false);
  for (auto consumer : m_consumers)
  {
    consumer->markShapeDirty();
  }
}

SharedTextStyleLease::SharedTextStyleLease(
    SharedTextStyleLease&& other) noexcept
    : m_link(std::move(other.m_link))
    , m_style(other.m_style)
    , m_consumer(other.m_consumer)
{
  other.m_style = nullptr;
  other.m_consumer = nullptr;
}

SharedTextStyleLease& SharedTextStyleLease::operator=(
    SharedTextStyleLease&& other) noexcept
{
  if (this != &other)
  {
    release();
    m_link = std::move(other.m_link);
    m_style = other.m_style;
    m_consumer = other.m_consumer;
    other.m_style = nullptr;
    other.m_consumer = nullptr;
  }
  return *this;
}

void SharedTextStyleLease::release()
{
  if (get() != nullptr)
  {
    m_link->cache->release(m_style, m_consumer);
  }
  m_link = nullptr;
  m_style = nullptr;
  m_consumer = nullptr;
}
