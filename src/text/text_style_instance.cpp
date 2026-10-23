#include "rive/text/text_style_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/text/text_interface.hpp"
#include "rive/text/text_style_global.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"
#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol_list_index.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"

using namespace rive;

namespace
{
// The kinds of value an instance's copy of a handle's values keeps in step.
enum class InputKind : uint8_t
{
  none,
  number,
  boolean,
  string,
  color,
  enumValue,
  trigger,
  symbolListIndex,
};

InputKind inputKind(const ViewModelInstanceValue* value)
{
  switch (value->coreType())
  {
    case ViewModelInstanceNumberBase::typeKey:
      return InputKind::number;
    case ViewModelInstanceBooleanBase::typeKey:
      return InputKind::boolean;
    case ViewModelInstanceStringBase::typeKey:
      return InputKind::string;
    case ViewModelInstanceColorBase::typeKey:
      return InputKind::color;
    case ViewModelInstanceEnumBase::typeKey:
      return InputKind::enumValue;
    case ViewModelInstanceTriggerBase::typeKey:
      return InputKind::trigger;
    case ViewModelInstanceSymbolListIndexBase::typeKey:
      return InputKind::symbolListIndex;
    default:
      return InputKind::none;
  }
}

// Copies [from]'s value into [to] (both of [kind]).
void copyValue(
    InputKind kind,
    ViewModelInstanceValue* from,
    ViewModelInstanceValue* to)
{
  switch (kind)
  {
    case InputKind::number:
      to->as<ViewModelInstanceNumber>()->propertyValue(
          from->as<ViewModelInstanceNumber>()->propertyValue());
      break;
    case InputKind::boolean:
      to->as<ViewModelInstanceBoolean>()->propertyValue(
          from->as<ViewModelInstanceBoolean>()->propertyValue());
      break;
    case InputKind::string:
      to->as<ViewModelInstanceString>()->propertyValue(
          from->as<ViewModelInstanceString>()->propertyValue());
      break;
    case InputKind::color:
      to->as<ViewModelInstanceColor>()->propertyValue(
          from->as<ViewModelInstanceColor>()->propertyValue());
      break;
    case InputKind::enumValue:
      to->as<ViewModelInstanceEnum>()->propertyValue(
          from->as<ViewModelInstanceEnum>()->propertyValue());
      break;
    case InputKind::trigger:
      to->as<ViewModelInstanceTrigger>()->propertyValue(
          from->as<ViewModelInstanceTrigger>()->propertyValue());
      break;
    case InputKind::symbolListIndex:
      to->as<ViewModelInstanceSymbolListIndex>()->propertyValue(
          from->as<ViewModelInstanceSymbolListIndex>()->propertyValue());
      break;
    case InputKind::none:
      break;
  }
}

bool isInput(ViewModelInstanceValue* from, ViewModelInstanceValue* to)
{
  auto property = from->viewModelProperty();
  if (property == nullptr)
  {
    property = to->viewModelProperty();
  }
  return property != nullptr && property->isInput();
}
} // namespace

TextStyleInstance::TextStyleInstance(
    const Artboard* source,
    Factory* factory,
    rcp<ViewModelInstance> viewModelInstance,
    bool readsOutsideOwnViewModel,
    TextInterface* text,
    Text* servedText,
    const TextStyleReference* servedStyle)
    : m_viewModelInstance(std::move(viewModelInstance))
    , m_text(text)
    , m_readsOutsideOwnViewModel(readsOutsideOwnViewModel)
{
  m_instance = source->instance<ArtboardInstance>(factory);
  m_paint = TextStyleGlobal::firstTextStylePaint(m_instance.get());
  if (m_paint != nullptr)
  {
    m_paint->owner(this);
  }
  // Run the style artboard's default state machine (if any) so the style
  // can animate.
  m_stateMachine = m_instance->defaultStateMachine();
  if (m_stateMachine != nullptr && servedText != nullptr &&
      servedStyle != nullptr)
  {
    m_stateMachine->serveTextStyle(servedText, servedStyle);
  }
}

TextStyleInstance::~TextStyleInstance()
{
  // The state machine references the instance, whose data binds reference
  // the view model instance: tear down in that order.
  m_inputs.clear();
  m_valuesSource = nullptr;
  m_stateMachine = nullptr;
  m_instance = nullptr;
  m_viewModelInstance = nullptr;
  m_listItem = nullptr;
  m_parent = nullptr;
}

void TextStyleInstance::bind(rcp<DataContext> parent, bool force)
{
  // A host re-resolving its binds keeps its context but may have swapped the
  // instances in it: a style reading the host's data resolves again too.
  if (m_isBound && !force && parent == m_parent && !m_readsOutsideOwnViewModel)
  {
    return;
  }
  m_isBound = true;
  m_parent = parent;
  if (m_listItem != nullptr)
  {
    // The list item sits between the style's own view model and the host.
    auto itemContext = make_rcp<DataContext>(m_listItem);
    itemContext->parent(parent);
    parent = itemContext;
  }
  if (m_viewModelInstance != nullptr)
  {
    m_instance->bindViewModelInstances({m_viewModelInstance}, parent);
  }
  else if (parent != nullptr)
  {
    // No view model of its own: read the host's data directly.
    m_instance->internalDataContext(parent);
  }
  if (m_stateMachine != nullptr)
  {
    m_stateMachine->dataContext(m_instance->dataContext());
  }
}

void TextStyleInstance::relink()
{
  if (m_readsOutsideOwnViewModel)
  {
    bind(m_parent, true);
  }
}

void TextStyleInstance::listItem(rcp<ViewModelInstance> value)
{
  if (value == m_listItem)
  {
    return;
  }
  m_listItem = std::move(value);
  if (m_isBound)
  {
    bind(m_parent, true);
    flush();
  }
}

bool TextStyleInstance::flush()
{
  m_instance->updateDataBinds();
  if (!m_instance->hasDirt(ComponentDirt::Components))
  {
    return false;
  }
  // The style's paint lives under its own artboard, not the Text: flush its
  // graph now and re-shape the Text (font size, spacing, font may change).
  m_instance->updatePass(false);
  if (m_text != nullptr)
  {
    m_text->markShapeDirty();
  }
  return true;
}

bool TextStyleInstance::advance(float elapsedSeconds, AdvanceFlags flags)
{
  // The values source's exposed inputs may have changed (its host's binds).
  syncInputs();
  bool keepGoing = false;
  bool newFrame = (flags & AdvanceFlags::NewFrame) == AdvanceFlags::NewFrame;
  bool advanceNested = (flags & AdvanceFlags::AdvanceNested) ==
                       AdvanceFlags::AdvanceNested;
  if (advanceNested && m_stateMachine != nullptr &&
      m_stateMachine->advance(elapsedSeconds, newFrame))
  {
    keepGoing = true;
  }
  if (m_instance->advanceInternal(
          elapsedSeconds,
          flags & ~AdvanceFlags::IsRoot))
  {
    keepGoing = true;
  }
  flush();
  return keepGoing;
}

AdvancingComponent::QuietState TextStyleInstance::quietState() const
{
  // A state machine, or binds reading data the host can change at any time
  // without waking this Text's row, means advancing may always have work.
  if (m_stateMachine != nullptr || m_readsOutsideOwnViewModel)
  {
    return AdvancingComponent::QuietState::never;
  }
  return m_instance->hasDirt(ComponentDirt::Components)
             ? AdvancingComponent::QuietState::busy
             : AdvancingComponent::QuietState::quiet;
}

void TextStyleInstance::styleShapeChanged()
{
  if (m_text != nullptr)
  {
    m_text->markShapeDirty();
  }
}

void TextStyleInstance::copyValuesFrom(rcp<ViewModelInstance> source)
{
  m_inputs.clear();
  m_valuesSource = std::move(source);
  if (m_valuesSource == nullptr || m_viewModelInstance == nullptr)
  {
    return;
  }
  auto& targets = m_viewModelInstance->propertyValues();
  for (auto& from : m_valuesSource->propertyValues())
  {
    auto kind = inputKind(from.get());
    if (kind == InputKind::none)
    {
      continue;
    }
    for (auto& to : targets)
    {
      if (to->viewModelPropertyId() != from->viewModelPropertyId() ||
          to->coreType() != from->coreType())
      {
        continue;
      }
      copyValue(kind, from.get(), to.get());
      if (isInput(from.get(), to.get()))
      {
        InputLink link;
        link.from = from;
        link.to = to;
        m_inputs.push_back(std::move(link));
      }
      break;
    }
  }
  // Remember what was copied.
  syncInputs();
}

bool TextStyleInstance::syncInputs()
{
  bool changed = false;
  for (auto& link : m_inputs)
  {
    auto from = link.from.get();
    auto kind = inputKind(from);
    bool differs = false;
    switch (kind)
    {
      case InputKind::number:
      {
        auto value = from->as<ViewModelInstanceNumber>()->propertyValue();
        differs = value != link.number;
        link.number = value;
        break;
      }
      case InputKind::boolean:
      {
        auto value = from->as<ViewModelInstanceBoolean>()->propertyValue();
        differs = value != link.boolean;
        link.boolean = value;
        break;
      }
      case InputKind::string:
      {
        auto& value = from->as<ViewModelInstanceString>()->propertyValue();
        differs = value != link.string;
        link.string = value;
        break;
      }
      case InputKind::color:
      {
        auto value = from->as<ViewModelInstanceColor>()->propertyValue();
        differs = value != link.color;
        link.color = value;
        break;
      }
      case InputKind::enumValue:
      {
        auto value = from->as<ViewModelInstanceEnum>()->propertyValue();
        differs = value != link.index;
        link.index = value;
        break;
      }
      case InputKind::trigger:
      {
        auto value = from->as<ViewModelInstanceTrigger>()->propertyValue();
        differs = value != link.index;
        link.index = value;
        break;
      }
      case InputKind::symbolListIndex:
      {
        auto value = from->as<ViewModelInstanceSymbolListIndex>()
                         ->propertyValue();
        differs = value != link.index;
        link.index = value;
        break;
      }
      case InputKind::none:
        break;
    }
    if (differs)
    {
      copyValue(kind, from, link.to.get());
      changed = true;
    }
  }
  return changed;
}
