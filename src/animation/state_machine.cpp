#include "rive/animation/state_machine.hpp"
#include "rive/animation/layer_state.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/artboard.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/state_machine_input.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/scripted/scripted_interpolator.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/animation/layer_state.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/animation/layer_state_flags.hpp"
#ifndef WITH_RIVE_EDITOR
#include "rive/animation/transition_condition.hpp"
#include "rive/animation/transition_property_viewmodel_comparator.hpp"
#include "rive/animation/transition_self_comparator.hpp"
#include "rive/animation/transition_value_artboard_comparator.hpp"
#include "rive/animation/transition_value_asset_comparator.hpp"
#include "rive/animation/transition_value_boolean_comparator.hpp"
#include "rive/animation/transition_value_color_comparator.hpp"
#include "rive/animation/transition_value_enum_comparator.hpp"
#include "rive/animation/transition_value_number_comparator.hpp"
#include "rive/animation/transition_value_string_comparator.hpp"
#include "rive/animation/transition_value_trigger_comparator.hpp"
#include "rive/animation/transition_viewmodel_condition.hpp"
#include "rive/data_bind/bindable_property_boolean.hpp"
#include "rive/data_bind/bindable_property_color.hpp"
#include "rive/data_bind/bindable_property_enum.hpp"
#include "rive/data_bind/bindable_property_integer.hpp"
#include "rive/data_bind/bindable_property_number.hpp"
#include "rive/data_bind/bindable_property_string.hpp"
#include "rive/data_bind/bindable_property_trigger.hpp"
#include "rive/data_bind_flags.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/blend_animation.hpp"
#include "rive/animation/blend_state.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/deferred_decode_lock.hpp"
#include "rive/generated/bones/bone_base.hpp"
#include "rive/generated/bones/root_bone_base.hpp"
#include "rive/generated/constraints/constraint_base.hpp"
#include "rive/generated/constraints/follow_path_constraint_base.hpp"
#include "rive/generated/constraints/ik_constraint_base.hpp"
#include "rive/generated/draw_rules_base.hpp"
#include "rive/generated/layout/layout_sizing_style_base.hpp"
#include "rive/generated/shapes/clipping_shape_base.hpp"
#include "rive/generated/solo_base.hpp"
#include "rive/generated/text/text_value_run_base.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/generated/layout/layout_component_style_base.hpp"
#include "rive/generated/layout_component_base.hpp"
#include "rive/generated/node_base.hpp"
#include "rive/generated/shapes/cubic_asymmetric_vertex_base.hpp"
#include "rive/generated/shapes/cubic_detached_vertex_base.hpp"
#include "rive/generated/shapes/cubic_mirrored_vertex_base.hpp"
#include "rive/generated/shapes/paint/color_channels_base.hpp"
#include "rive/generated/shapes/paint/feather_base.hpp"
#include "rive/generated/shapes/paint/gradient_stop_base.hpp"
#include "rive/generated/shapes/paint/solid_color_base.hpp"
#include "rive/generated/shapes/paint/stroke_base.hpp"
#include "rive/generated/shapes/paint/trim_path_base.hpp"
#include "rive/generated/shapes/parametric_path_base.hpp"
#include "rive/generated/shapes/straight_vertex_base.hpp"
#include "rive/generated/shapes/vertex_base.hpp"
#include "rive/generated/transform_component_base.hpp"
#include "rive/generated/world_transform_component_base.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#endif

#include <algorithm>

using namespace rive;

#ifndef WITH_RIVE_EDITOR
// Settle-safety, see LayerState::transitionsSettleSafe. A comparator is tracked
// when every value it reads can only change through a dirty event on one of the
// instance's own data binds, or a change of its data context: both unsettle the
// instance's layers. Exact type keys on purpose, so a new condition or
// comparator type starts out untracked (always searched) rather than
// inheriting its parent's answer. `untracked` holds the objects that state
// machine binds write without going through that path.
static bool isTrackedComparator(
    TransitionComparator* comparator,
    const std::unordered_set<const Core*>& untracked)
{
  if (comparator == nullptr)
  {
    // A missing side compares as ConditionComparisonNone: always false.
    return true;
  }
  if (untracked.count(comparator) != 0)
  {
    return false;
  }
  switch (comparator->coreType())
  {
    case TransitionPropertyViewModelComparatorBase::typeKey:
    {
      auto bindable = comparator->as<TransitionPropertyViewModelComparator>()
                          ->bindableProperty();
      if (bindable == nullptr || untracked.count(bindable) != 0)
      {
        return false;
      }
      switch (bindable->coreType())
      {
        case BindablePropertyNumberBase::typeKey:
        case BindablePropertyIntegerBase::typeKey:
        case BindablePropertyBooleanBase::typeKey:
        case BindablePropertyStringBase::typeKey:
        case BindablePropertyColorBase::typeKey:
        case BindablePropertyEnumBase::typeKey:
        case BindablePropertyTriggerBase::typeKey:
          return true;
        default:
          // Asset, artboard and view model bindables are also written
          // outside their binds' dirty path.
          return false;
      }
    }
    case TransitionValueNumberComparatorBase::typeKey:
    case TransitionValueBooleanComparatorBase::typeKey:
    case TransitionValueStringComparatorBase::typeKey:
    case TransitionValueColorComparatorBase::typeKey:
    case TransitionValueEnumComparatorBase::typeKey:
    case TransitionValueTriggerComparatorBase::typeKey:
    case TransitionValueAssetComparatorBase::typeKey:
    case TransitionValueArtboardComparatorBase::typeKey:
    case TransitionSelfComparatorBase::typeKey:
      return true;
    default:
      // Component and artboard property comparators read live values
      // that nothing reports to the state machine.
      return false;
  }
}

static bool isTrackedCondition(
    TransitionCondition* condition,
    const std::unordered_set<const Core*>& untracked)
{
  // Only view model conditions are tracked. State machine inputs, focus,
  // artboard and scripted conditions keep the full search.
  if (condition == nullptr ||
      condition->coreType() != TransitionViewModelConditionBase::typeKey)
  {
    return false;
  }
  auto viewModelCondition = condition->as<TransitionViewModelCondition>();
  return isTrackedComparator(viewModelCondition->leftComparator(), untracked) &&
         isTrackedComparator(viewModelCondition->rightComparator(), untracked);
}

static bool isSettleSafe(
    const LayerState* state,
    const std::unordered_set<const Core*>& untracked)
{
  // A random state can allow transitions and still take none (a draw of
  // exactly 1.0 falls through the weights), and must be searched again.
  if ((static_cast<LayerStateFlags>(state->flags()) &
       LayerStateFlags::Random) == LayerStateFlags::Random)
  {
    return false;
  }
  for (size_t i = 0, count = state->transitionCount(); i < count; i++)
  {
    auto transition = state->transition(i);
    if (transition == nullptr || transition->isDisabled())
    {
      continue;
    }
    for (size_t j = 0, conditions = transition->conditionCount();
         j < conditions;
         j++)
    {
      if (!isTrackedCondition(transition->condition(j), untracked))
      {
        return false;
      }
    }
  }
  return true;
}

// Whether a view model condition on one of `state`'s transitions answers yes
// to `reads`.
static bool anyViewModelCondition(
    const LayerState* state,
    bool (TransitionViewModelCondition::*reads)() const)
{
  for (size_t i = 0, count = state->transitionCount(); i < count; i++)
  {
    auto transition = state->transition(i);
    if (transition == nullptr)
    {
      continue;
    }
    for (size_t j = 0, conditions = transition->conditionCount();
         j < conditions;
         j++)
    {
      auto condition = transition->condition(j);
      if (condition != nullptr &&
          condition->coreType() == TransitionViewModelConditionBase::typeKey &&
          (condition->as<TransitionViewModelCondition>()->*reads)())
      {
        return true;
      }
    }
  }
  return false;
}

// Only the flags are read here, since a layer's animations and target states
// may not be resolved yet when this runs (see onAddedClean).
static bool transitionsIgnoreTime(const LayerState* state)
{
  for (size_t i = 0, count = state->transitionCount(); i < count; i++)
  {
    auto transition = state->transition(i);
    if (transition != nullptr && !transition->isDisabled() &&
        transition->enableExitTime())
    {
      return false;
    }
  }
  return true;
}
#endif

StateMachine::StateMachine() {}

StateMachine::~StateMachine() {}

StatusCode StateMachine::onAddedDirty(CoreContext* context)
{
  StatusCode code;
  for (auto& object : m_Inputs)
  {
    if ((code = object->onAddedDirty(context)) != StatusCode::Ok)
    {
      return code;
    }
  }
  for (auto& object : m_Layers)
  {
    if ((code = object->onAddedDirty(context)) != StatusCode::Ok)
    {
      return code;
    }
  }
  for (auto& object : m_Listeners)
  {
    if ((code = object->onAddedDirty(context)) != StatusCode::Ok)
    {
      return code;
    }
  }
  return StatusCode::Ok;
}

#ifndef WITH_RIVE_EDITOR
// See LayerState::transitionsReadObjects. Any view model condition counts,
// subclasses (artboard and focus conditions) included.
static bool transitionsReadObjects(const LayerState* state)
{
  for (size_t i = 0, count = state->transitionCount(); i < count; i++)
  {
    auto transition = state->transition(i);
    if (transition == nullptr)
    {
      continue;
    }
    for (size_t j = 0, conditions = transition->conditionCount();
         j < conditions;
         j++)
    {
      auto condition = transition->condition(j);
      if (condition != nullptr &&
          condition->is<TransitionViewModelCondition>() &&
          condition->as<TransitionViewModelCondition>()->readsObjectProperty())
      {
        return true;
      }
    }
  }
  return false;
}

// How a property several layers key is collected and set once a frame, or
// false when it isn't. Setting it only to its final value skips the
// intermediate sets, so its setter must do nothing those would have needed:
// store the value and invalidate (mark dirt, call changed(), mark a layout
// node dirty, re-collapse a Solo's children), from which everything is
// recomputed off the final value. Each key is checked against every override
// of its hook. A property that isn't listed keeps being set by each layer, as
// before. Left out on purpose:
// - positionLeft/Top/Right/Bottom and positionType, whose hook writes them;
// - a layout's scale types, whose hook sets its intrinsicallySizedValue from
//   them, over what a bind or a layer set it to;
// - a layout's direction, whose hook clears the layout animation it
//   inherits, which a later layer's width and height keyframes read to pick
//   their mix;
// - a text run's style id, whose hook keeps the current style when the id
//   names none, so its result depends on the intermediate ids;
// - nested inputs and nested animations' controls, custom properties and
//   scripts' inputs, whose setters reach other machines and scripts;
// - Joystick x/y, applied by the joystick itself;
// - numbers kept as bits or computed (the scroll and bitmask properties),
//   and uints that interpolate (color channels).
static bool layerAccumulatedKind(uint16_t key, BlendAccumulator::Kind& kind)
{
  using Kind = BlendAccumulator::Kind;
  switch (key)
  {
    // xArtboard and yArtboard are counted as these (see
    // BlendAccumulator::storageKey).
    case NodeBase::xPropertyKey:
    case NodeBase::yPropertyKey:
    case TransformComponentBase::rotationPropertyKey:
    case TransformComponentBase::scaleXPropertyKey:
    case TransformComponentBase::scaleYPropertyKey:
    case WorldTransformComponentBase::opacityPropertyKey:
    case LayoutComponentBase::widthPropertyKey:
    case LayoutComponentBase::heightPropertyKey:
    case LayoutComponentStyleBase::cornerRadiusTLPropertyKey:
    case LayoutComponentStyleBase::cornerRadiusTRPropertyKey:
    case LayoutComponentStyleBase::cornerRadiusBLPropertyKey:
    case LayoutComponentStyleBase::cornerRadiusBRPropertyKey:
    case ParametricPathBase::widthPropertyKey:
    case ParametricPathBase::heightPropertyKey:
    case VertexBase::xPropertyKey:
    case VertexBase::yPropertyKey:
    case StraightVertexBase::radiusPropertyKey:
    case CubicAsymmetricVertexBase::rotationPropertyKey:
    case CubicAsymmetricVertexBase::inDistancePropertyKey:
    case CubicAsymmetricVertexBase::outDistancePropertyKey:
    case CubicMirroredVertexBase::rotationPropertyKey:
    case CubicMirroredVertexBase::distancePropertyKey:
    case CubicDetachedVertexBase::inRotationPropertyKey:
    case CubicDetachedVertexBase::inDistancePropertyKey:
    case CubicDetachedVertexBase::outRotationPropertyKey:
    case CubicDetachedVertexBase::outDistancePropertyKey:
    case GradientStopBase::positionPropertyKey:
    case StrokeBase::thicknessPropertyKey:
    case TrimPathBase::startPropertyKey:
    case TrimPathBase::endPropertyKey:
    case TrimPathBase::offsetPropertyKey:
    case FeatherBase::strengthPropertyKey:
    case BoneBase::lengthPropertyKey:
    case RootBoneBase::xPropertyKey:
    case RootBoneBase::yPropertyKey:
    case ConstraintBase::strengthPropertyKey:
    case FollowPathConstraintBase::distancePropertyKey:
      kind = Kind::number;
      return true;
    case SolidColorBase::colorValuePropertyKey:
    case GradientStopBase::colorValuePropertyKey:
      kind = Kind::color;
      return true;
    case SoloBase::activeComponentIdPropertyKey:
    case DrawRulesBase::drawTargetIdPropertyKey:
      kind = Kind::id;
      return true;
    case LayoutSizingStyleBase::displayValuePropertyKey:
    case LayoutComponentStyleBase::layoutAlignmentTypePropertyKey:
      kind = Kind::uint;
      return true;
    case ClippingShapeBase::isVisiblePropertyKey:
    case FeatherBase::innerPropertyKey:
    case FollowPathConstraintBase::orientPropertyKey:
    case IKConstraintBase::invertDirectionPropertyKey:
      kind = Kind::boolean;
      return true;
    case TextValueRunBase::textPropertyKey:
      kind = Kind::string;
      return true;
  }
  return false;
}

// Whether a layer keys another property of the object that key's value
// depends on. Its set at the flush would then land out of order with that
// property's writes.
static bool entangledWithKeyed(
    uint32_t objectId,
    uint16_t key,
    const std::map<std::pair<uint32_t, uint16_t>, int>& layersKeying)
{
  auto keyed = [&](uint16_t other) {
    return layersKeying.count({objectId, other}) != 0;
  };
  switch (key)
  {
    // The color channels read and write colorValue: one keyed alongside it
    // would mix from a stale color.
    case SolidColorBase::colorValuePropertyKey:
    case GradientStopBase::colorValuePropertyKey:
      return keyed(ColorChannelsBase::colorRedPropertyKey) ||
             keyed(ColorChannelsBase::colorGreenPropertyKey) ||
             keyed(ColorChannelsBase::colorBluePropertyKey) ||
             keyed(ColorChannelsBase::colorAlphaPropertyKey);
  }
  return false;
}

static void addKeyed(
    const LinearAnimation* animation,
    std::set<std::pair<uint32_t, uint16_t>>& keyed)
{
  if (animation == nullptr)
  {
    return;
  }
  // Without decoding a deferred body: this runs as the machine's first
  // instance is made, before any of its states play.
  animation->forEachKeyedProperty([&](uint32_t objectId, uint32_t propertyKey) {
    keyed.insert(
        {objectId, (uint16_t)BlendAccumulator::storageKey((int)propertyKey)});
  });
}

const std::vector<StateMachine::LayerConflict>& StateMachine::layerConflicts()
    const
{
  if (m_layerConflictsReady.load(std::memory_order_acquire))
  {
    return m_layerConflicts;
  }
  // The decode lock first: reading deferred bodies takes it
  // (LinearAnimation::forEachKeyedProperty), and a decode on this thread may
  // already hold it, so the two are always taken in this order.
  std::lock_guard<std::recursive_mutex> decodeLock(deferredDecodeMutex());
  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);
  if (m_layerConflictsReady.load(std::memory_order_relaxed))
  {
    return m_layerConflicts;
  }
  // How many layers key each (object, property).
  std::map<std::pair<uint32_t, uint16_t>, int> layersKeying;
  for (auto& layer : m_Layers)
  {
    std::set<std::pair<uint32_t, uint16_t>> keyed;
    for (size_t i = 0, count = layer->stateCount(); i < count; i++)
    {
      const LayerState* state = layer->state(i);
      if (state == nullptr)
      {
        continue;
      }
      if (state->is<AnimationState>())
      {
        addKeyed(state->as<AnimationState>()->animation(), keyed);
      }
      else if (state->is<BlendState>())
      {
        for (const BlendAnimation* blend :
             state->as<BlendState>()->animations())
        {
          addKeyed(blend->animation(), keyed);
        }
      }
    }
    for (const auto& entry : keyed)
    {
      layersKeying[entry]++;
    }
  }
  for (const auto& entry : layersKeying)
  {
    const uint32_t objectId = entry.first.first;
    const uint16_t key = entry.first.second;
    BlendAccumulator::Kind kind;
    if (entry.second < 2 || !layerAccumulatedKind(key, kind) ||
        entangledWithKeyed(objectId, key, layersKeying))
    {
      continue;
    }
    m_layerConflicts.push_back({objectId, key, kind});
  }
  m_layerConflictsReady.store(true, std::memory_order_release);
  return m_layerConflicts;
}

// The bindable's type and the value it holds before (or without) a bind, as
// a key, for the types whose instances can share. False for the others.
static bool bindableShareKey(
    const BindableProperty* bindable,
    std::string& value)
{
  auto put = [&value](const void* bytes, size_t size) {
    value.assign(static_cast<const char*>(bytes), size);
  };
  switch (bindable->coreType())
  {
    case BindablePropertyNumberBase::typeKey:
    {
      float v = bindable->as<BindablePropertyNumber>()->propertyValue();
      put(&v, sizeof(v));
      return true;
    }
    case BindablePropertyIntegerBase::typeKey:
    {
      uint32_t v = bindable->as<BindablePropertyInteger>()->propertyValue();
      put(&v, sizeof(v));
      return true;
    }
    case BindablePropertyBooleanBase::typeKey:
    {
      bool v = bindable->as<BindablePropertyBoolean>()->propertyValue();
      put(&v, sizeof(v));
      return true;
    }
    case BindablePropertyColorBase::typeKey:
    {
      int v = bindable->as<BindablePropertyColor>()->propertyValue();
      put(&v, sizeof(v));
      return true;
    }
    case BindablePropertyEnumBase::typeKey:
    {
      auto v = bindable->as<BindablePropertyEnum>()->propertyValue();
      put(&v, sizeof(v));
      return true;
    }
    case BindablePropertyStringBase::typeKey:
      value = bindable->as<BindablePropertyString>()->propertyValue();
      return true;
    default:
      return false;
  }
}

const std::vector<int32_t>& StateMachine::bindableTwins() const
{
  if (m_bindableTwinsReady.load(std::memory_order_acquire))
  {
    return m_bindableTwins;
  }
  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);
  if (m_bindableTwinsReady.load(std::memory_order_relaxed))
  {
    return m_bindableTwins;
  }
  // A bindable shares only if one bind writes it, and that bind is a plain
  // source-to-target one: a listener action's bindable also takes the
  // action's input and writes it back through a bind to source.
  const auto unshared = DataBindFlags::ToSource | DataBindFlags::TwoWay |
                        DataBindFlags::NameBased;
  std::unordered_map<const Core*, int> bindsInto;
  for (auto& dataBind : m_dataBinds)
  {
    if (dataBind->target() != nullptr)
    {
      bindsInto[dataBind->target()]++;
    }
  }
  using Key = std::
      tuple<uint16_t, uint32_t, uint32_t, std::vector<uint32_t>, std::string>;
  std::map<Key, int32_t> first;
  std::vector<int32_t> twins(m_dataBinds.size(), -1);
  bool anyTwins = false;
  std::string value;
  for (size_t i = 0; i < m_dataBinds.size(); i++)
  {
    const DataBind* dataBind = m_dataBinds[i].get();
    const Core* target = dataBind->target();
    if (target == nullptr || !target->is<BindableProperty>() ||
        bindsInto[target] != 1 || !dataBind->is<DataBindContext>() ||
        dataBind->converter() != nullptr ||
        (static_cast<DataBindFlags>(dataBind->flags()) & unshared) !=
            DataBindFlags::None)
    {
      continue;
    }
    const auto& path = dataBind->as<DataBindContext>()->sourcePathIds();
    if (path.empty() ||
        !bindableShareKey(target->as<BindableProperty>(), value))
    {
      continue;
    }
    Key key{
        target->coreType(),
        dataBind->flags(),
        dataBind->propertyKey(),
        path,
        value};
    auto found = first.find(key);
    if (found == first.end())
    {
      first.emplace(std::move(key), static_cast<int32_t>(i));
      continue;
    }
    twins[i] = found->second;
    anyTwins = true;
  }
  if (anyTwins)
  {
    m_bindableTwins = std::move(twins);
  }
  m_bindableTwinsReady.store(true, std::memory_order_release);
  return m_bindableTwins;
}
#endif

StatusCode StateMachine::onAddedClean(CoreContext* context)
{
  StatusCode code;
  for (auto& object : m_Inputs)
  {
    if ((code = object->onAddedClean(context)) != StatusCode::Ok)
    {
      return code;
    }
  }
  for (auto& object : m_Layers)
  {
    if ((code = object->onAddedClean(context)) != StatusCode::Ok)
    {
      return code;
    }
  }
  for (auto& object : m_Listeners)
  {
    if ((code = object->onAddedClean(context)) != StatusCode::Ok)
    {
      return code;
    }
  }
#ifndef WITH_RIVE_EDITOR
  // Classified here, once the whole artboard has been read, from what
  // reading attaches: conditions, comparators, bindables and flags. Importers
  // resolve later still (a layer's only when the next layer arrives, which
  // can be in the next artboard), so nothing they fill in, like a state's
  // animation, is read here. Bindables fed once never report later source
  // changes, and binds aimed at anything but a bindable write the shared
  // definition (a comparator's constant, say) directly.
  std::unordered_set<const Core*> untracked;
  for (auto& dataBind : m_dataBinds)
  {
    auto target = dataBind->target();
    bool bindsOnce = enums::is_flag_set(
        static_cast<DataBindFlags>(dataBind->flags()),
        DataBindFlags::Once);
    if (target != nullptr && (bindsOnce || !target->is<BindableProperty>()))
    {
      untracked.insert(target);
    }
  }
  for (auto& layer : m_Layers)
  {
    for (size_t i = 0, count = layer->stateCount(); i < count; i++)
    {
      auto state = layer->state(i);
      if (state != nullptr)
      {
        state->transitionsSettleSafe(isSettleSafe(state, untracked));
        state->transitionsIgnoreTime(transitionsIgnoreTime(state));
        state->transitionsReadObjects(transitionsReadObjects(state));
        m_readsComponentTriggers = m_readsComponentTriggers ||
                                   anyViewModelCondition(
                                       state,
                                       &TransitionViewModelCondition::
                                           readsComponentTrigger);
        m_readsPendingChanges = m_readsPendingChanges ||
                                anyViewModelCondition(
                                    state,
                                    &TransitionViewModelCondition::
                                        readsPendingChange);
        addScriptedInterpolators(state);
      }
    }
  }
#endif
  return StatusCode::Ok;
}

// A scripted interpolator on a transition runs per StateMachineInstance, like
// the machine's other scripted objects: registering it here makes every
// instance clone it (inputs bound against that instance) and initialize it.
// Transitions resolved their interpolator in onAddedDirty. Several
// transitions can share one, so each is registered once.
void StateMachine::addScriptedInterpolators(const LayerState* state)
{
  for (size_t i = 0, count = state->transitionCount(); i < count; i++)
  {
    auto transition = state->transition(i);
    if (transition == nullptr)
    {
      continue;
    }
    auto interpolator = transition->interpolator();
    if (interpolator == nullptr || !interpolator->is<ScriptedInterpolator>())
    {
      continue;
    }
    ScriptedObject* scripted = interpolator->as<ScriptedInterpolator>();
    if (std::find(
            m_scriptedObjects.begin(),
            m_scriptedObjects.end(),
            scripted) == m_scriptedObjects.end())
    {
      addScriptedObject(scripted);
    }
  }
}

StatusCode StateMachine::import(ImportStack& importStack)
{
  auto artboardImporter = importStack.latest<ArtboardImporter>(
      ArtboardBase::typeKey);
  if (artboardImporter == nullptr)
  {
    return StatusCode::MissingObject;
  }
  artboardImporter->addStateMachine(this);
  return Super::import(importStack);
}

void StateMachine::addLayer(std::unique_ptr<StateMachineLayer> layer)
{
  m_Layers.push_back(std::move(layer));
}

void StateMachine::addInput(std::unique_ptr<StateMachineInput> input)
{
  m_Inputs.push_back(std::move(input));
}

void StateMachine::addListener(std::unique_ptr<StateMachineListener> listener)
{
  m_Listeners.push_back(std::move(listener));
}

void StateMachine::addDataBind(std::unique_ptr<DataBind> dataBind)
{
  m_dataBinds.push_back(std::move(dataBind));
}

const StateMachineInput* StateMachine::input(std::string name) const
{
  for (auto& input : m_Inputs)
  {
    if (input->name() == name)
    {
      return input.get();
    }
  }
#ifdef WITH_RIVE_EDITOR
  for (auto* input : m_editorInputs)
  {
    if (input != nullptr && input->name() == name)
    {
      return input;
    }
  }
#endif
  return nullptr;
}

const StateMachineInput* StateMachine::input(size_t index) const
{
  if (index < m_Inputs.size())
  {
    return m_Inputs[index].get();
  }
#ifdef WITH_RIVE_EDITOR
  if (m_Inputs.empty() && index < m_editorInputs.size())
  {
    return m_editorInputs[index];
  }
#endif
  return nullptr;
}

const StateMachineLayer* StateMachine::layer(std::string name) const
{
  for (auto& layer : m_Layers)
  {
    if (layer->name() == name)
    {
      return layer.get();
    }
  }
#ifdef WITH_RIVE_EDITOR
  for (auto* layer : m_editorLayers)
  {
    if (layer != nullptr && layer->name() == name)
    {
      return layer;
    }
  }
#endif
  return nullptr;
}

const StateMachineLayer* StateMachine::layer(size_t index) const
{
  if (index < m_Layers.size())
  {
    return m_Layers[index].get();
  }
#ifdef WITH_RIVE_EDITOR
  if (m_Layers.empty() && index < m_editorLayers.size())
  {
    return m_editorLayers[index];
  }
#endif
  return nullptr;
}

const StateMachineListener* StateMachine::listener(size_t index) const
{
  if (index < m_Listeners.size())
  {
    return m_Listeners[index].get();
  }
#ifdef WITH_RIVE_EDITOR
  if (m_Listeners.empty() && index < m_editorListeners.size())
  {
    return m_editorListeners[index];
  }
#endif
  return nullptr;
}

const DataBind* StateMachine::dataBind(size_t index) const
{
  if (index < m_dataBinds.size())
  {
    return m_dataBinds[index].get();
  }
#ifdef WITH_RIVE_EDITOR
  if (m_dataBinds.empty() && index < m_editorDataBinds.size())
  {
    return m_editorDataBinds[index];
  }
#endif
  return nullptr;
}

void StateMachine::addScriptedObject(ScriptedObject* object)
{
  m_scriptedObjects.push_back(object);
}