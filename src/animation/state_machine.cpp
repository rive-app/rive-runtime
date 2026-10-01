#include "rive/animation/state_machine.hpp"
#include "rive/artboard.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/state_machine_input.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/data_bind/data_bind.hpp"
#ifndef WITH_RIVE_EDITOR
#include "rive/animation/layer_state.hpp"
#include "rive/animation/layer_state_flags.hpp"
#include "rive/animation/state_transition.hpp"
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
#include <unordered_set>
#endif

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
            auto bindable =
                comparator->as<TransitionPropertyViewModelComparator>()
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

static bool isTrackedCondition(TransitionCondition* condition,
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
    return isTrackedComparator(viewModelCondition->leftComparator(),
                               untracked) &&
           isTrackedComparator(viewModelCondition->rightComparator(),
                               untracked);
}

static bool isSettleSafe(const LayerState* state,
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
        bool bindsOnce =
            enums::is_flag_set(static_cast<DataBindFlags>(dataBind->flags()),
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
            }
        }
    }
#endif
    return StatusCode::Ok;
}

StatusCode StateMachine::import(ImportStack& importStack)
{
    auto artboardImporter =
        importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
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