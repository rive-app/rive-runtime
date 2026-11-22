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
  // Tearing down is no change for the host to wake to.
  m_instance->quietOwner(nullptr);
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
  using QuietState = AdvancingComponent::QuietState;
  // Exposed inputs still to copy from the values source. It changes while
  // the row hosting this Text is awake: the host's binds write it, and so
  // does the handle's own instance as it advances.
  if (inputsChanged())
  {
    return QuietState::busy;
  }
  // Once quiet, whatever may change the style wakes the row hosting its Text
  // (see markQuiet), so the machine and the artboard answer as a row's do:
  // the machine first, as a list checks its rows' (see
  // ArtboardComponentList::rowQuietState).
  if (m_stateMachine != nullptr)
  {
    auto state = m_stateMachine->rowQuietState();
    if (state != QuietState::quiet)
    {
      return state;
    }
  }
  return m_instance->rowQuietState();
}

void TextStyleInstance::markQuiet(Artboard* host)
{
  // The style's artboard is no row of anything; any value other than
  // kNoQuietRow marks it.
  m_instance->quietOwner(host);
  m_instance->quietHostRow(0);
  m_instance->markHostedQuiet();
}

bool TextStyleInstance::markedQuiet() const
{
  return m_instance->quietHostRow() != Artboard::kNoQuietRow;
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

bool TextStyleInstance::InputLink::differs() const
{
  auto value = from.get();
  switch (inputKind(value))
  {
    case InputKind::number:
      return value->as<ViewModelInstanceNumber>()->propertyValue() != number;
    case InputKind::boolean:
      return value->as<ViewModelInstanceBoolean>()->propertyValue() != boolean;
    case InputKind::string:
      return value->as<ViewModelInstanceString>()->propertyValue() != string;
    case InputKind::color:
      return value->as<ViewModelInstanceColor>()->propertyValue() != color;
    case InputKind::enumValue:
      return value->as<ViewModelInstanceEnum>()->propertyValue() != index;
    case InputKind::trigger:
      return value->as<ViewModelInstanceTrigger>()->propertyValue() != index;
    case InputKind::symbolListIndex:
      return value->as<ViewModelInstanceSymbolListIndex>()->propertyValue() !=
             index;
    case InputKind::none:
      break;
  }
  return false;
}

void TextStyleInstance::InputLink::record()
{
  auto value = from.get();
  switch (inputKind(value))
  {
    case InputKind::number:
      number = value->as<ViewModelInstanceNumber>()->propertyValue();
      break;
    case InputKind::boolean:
      boolean = value->as<ViewModelInstanceBoolean>()->propertyValue();
      break;
    case InputKind::string:
      string = value->as<ViewModelInstanceString>()->propertyValue();
      break;
    case InputKind::color:
      color = value->as<ViewModelInstanceColor>()->propertyValue();
      break;
    case InputKind::enumValue:
      index = value->as<ViewModelInstanceEnum>()->propertyValue();
      break;
    case InputKind::trigger:
      index = value->as<ViewModelInstanceTrigger>()->propertyValue();
      break;
    case InputKind::symbolListIndex:
      index = value->as<ViewModelInstanceSymbolListIndex>()->propertyValue();
      break;
    case InputKind::none:
      break;
  }
}

bool TextStyleInstance::inputsChanged() const
{
  for (auto& link : m_inputs)
  {
    if (link.differs())
    {
      return true;
    }
  }
  return false;
}

bool TextStyleInstance::syncInputs()
{
  bool changed = false;
  for (auto& link : m_inputs)
  {
    if (link.differs())
    {
      link.record();
      auto from = link.from.get();
      copyValue(inputKind(from), from, link.to.get());
      changed = true;
    }
  }
  return changed;
}
