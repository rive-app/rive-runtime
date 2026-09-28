#include "rive/animation/listener_viewmodel_change.hpp"
#include "rive/animation/listener_input_value.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/data_bind/bindable_property.hpp"
#include "rive/data_bind/bindable_property_boolean.hpp"
#include "rive/data_bind/bindable_property_number.hpp"
#include "rive/data_bind/bindable_property_string.hpp"
#include "rive/data_bind/bindable_property_viewmodel.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/importers/bindable_property_importer.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"

using namespace rive;

ListenerViewModelChange::~ListenerViewModelChange()
{
    if (m_bindableProperty != nullptr)
    {
        delete m_bindableProperty;
        m_bindableProperty = nullptr;
    }
}

StatusCode ListenerViewModelChange::import(ImportStack& importStack)
{

    auto bindablePropertyImporter =
        importStack.latest<BindablePropertyImporter>(
            BindablePropertyBase::typeKey);
    if (bindablePropertyImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    m_bindableProperty = bindablePropertyImporter->bindableProperty();

    return Super::import(importStack);
}

static const GamepadSnapshot* gamepadSnapshot(
    const ListenerInvocation& invocation)
{
    if (const GamepadEventInvocation* event = invocation.asGamepadEvent())
    {
        return &event->fullState;
    }
    if (const GamepadConnectedInvocation* connected =
            invocation.asGamepadConnected())
    {
        return &connected->snapshot;
    }
    return nullptr;
}

// Missing indices read as 0, matching standardGamepadAxisValue.
static float gamepadValueAt(const std::vector<float>& values, size_t index)
{
    return index < values.size() ? values[index] : 0.0f;
}

static bool applyNumber(BindableProperty* bindable, float value)
{
    if (!bindable->is<BindablePropertyNumber>())
    {
        return false;
    }
    bindable->as<BindablePropertyNumber>()->propertyValue(value);
    return true;
}

static bool applyBoolean(BindableProperty* bindable, bool value)
{
    if (!bindable->is<BindablePropertyBoolean>())
    {
        return false;
    }
    bindable->as<BindablePropertyBoolean>()->propertyValue(value);
    return true;
}

static bool applyString(BindableProperty* bindable, const std::string& value)
{
    if (!bindable->is<BindablePropertyString>())
    {
        return false;
    }
    bindable->as<BindablePropertyString>()->propertyValue(value);
    return true;
}

bool ListenerViewModelChange::applyInputValue(
    BindableProperty* bindableInstance,
    const ListenerInvocation& invocation) const
{
    const size_t index = inputValueIndex();
    switch (static_cast<ListenerInputValue>(inputValue()))
    {
        case ListenerInputValue::none:
            return true;
        case ListenerInputValue::pointerX:
        case ListenerInputValue::pointerY:
        case ListenerInputValue::pointerDeltaX:
        case ListenerInputValue::pointerDeltaY:
        {
            const PointerInvocation* pointer = invocation.asPointer();
            if (pointer == nullptr)
            {
                return false;
            }
            const Vec2D delta = pointer->position - pointer->previousPosition;
            switch (static_cast<ListenerInputValue>(inputValue()))
            {
                case ListenerInputValue::pointerX:
                    return applyNumber(bindableInstance, pointer->position.x);
                case ListenerInputValue::pointerY:
                    return applyNumber(bindableInstance, pointer->position.y);
                case ListenerInputValue::pointerDeltaX:
                    return applyNumber(bindableInstance, delta.x);
                default:
                    return applyNumber(bindableInstance, delta.y);
            }
        }
        case ListenerInputValue::keyPressed:
        {
            const KeyboardInvocation* keyboard = invocation.asKeyboard();
            return keyboard != nullptr &&
                   applyBoolean(bindableInstance, keyboard->isPressed);
        }
        case ListenerInputValue::text:
        {
            const TextInputInvocation* textInput = invocation.asTextInput();
            return textInput != nullptr &&
                   applyString(bindableInstance, textInput->text);
        }
        case ListenerInputValue::focused:
        {
            const FocusInvocation* focus = invocation.asFocus();
            return focus != nullptr &&
                   applyBoolean(bindableInstance, focus->isFocus);
        }
        case ListenerInputValue::gamepadButtonPressed:
        {
            const GamepadSnapshot* snapshot = gamepadSnapshot(invocation);
            return snapshot != nullptr &&
                   applyBoolean(bindableInstance,
                                index < 64 && (snapshot->buttonMask &
                                               (uint64_t(1) << index)) != 0);
        }
        case ListenerInputValue::gamepadButtonValue:
        {
            const GamepadSnapshot* snapshot = gamepadSnapshot(invocation);
            return snapshot != nullptr &&
                   applyNumber(bindableInstance,
                               gamepadValueAt(snapshot->buttonValues, index));
        }
        case ListenerInputValue::gamepadAxis:
        {
            const GamepadSnapshot* snapshot = gamepadSnapshot(invocation);
            return snapshot != nullptr &&
                   applyNumber(bindableInstance,
                               gamepadValueAt(snapshot->axes, index));
        }
        case ListenerInputValue::gamepadChangedValue:
        {
            const GamepadEventInvocation* event = invocation.asGamepadEvent();
            return event != nullptr &&
                   applyNumber(bindableInstance, event->change.value);
        }
    }
    return false;
}

// Note: perform works the same way whether the value comes from a direct value
// assignment or from another view model. In the case of coming from another
// view model, the state machine instance method "updataDataBinds" will handle
// updating the value of the bound object. That's the benefit of binding the
// same bindable property with two data binding objects.
void ListenerViewModelChange::perform(
    StateMachineInstance* stateMachineInstance,
    const ListenerInvocation& invocation) const
{
    // Get the bindable property instance from the state machine instance
    // context
    auto bindableInstance =
        stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
    // When the value comes from the listener's input, write it into the
    // bindable instance so the to-source bind below applies it (and its
    // converter). Skip the action if this invocation can't provide it.
    if (static_cast<ListenerInputValue>(inputValue()) !=
            ListenerInputValue::none &&
        (bindableInstance == nullptr ||
         !applyInputValue(bindableInstance, invocation)))
    {
        return;
    }
    // Get the data bound object (that goes from target to source) from this
    // bindable instance
    auto dataBind =
        stateMachineInstance->bindableDataBindToSource(bindableInstance);
    auto dataBindToTarget =
        stateMachineInstance->bindableDataBindToTarget(bindableInstance);
    if (dataBind != nullptr)
    {
        if (dataBind->target() != nullptr &&
            dataBind->target()->is<BindablePropertyViewModel>())
        {
            auto targetValue =
                dataBind->target()->as<BindablePropertyViewModel>();
            auto context = stateMachineInstance->dataContext();
            if (context != nullptr)
            {
                auto value = context->mainViewModelInstance().get();
                targetValue->viewModelInstanceValue(value);
                CoreRegistry::setUint(
                    targetValue,
                    BindablePropertyIdBase::propertyValuePropertyKey,
                    ViewModelInstance::pointerKey(value));
            }
        }
        dataBind->updateSourceBinding(true);
    }
    if (dataBindToTarget)
    {
        dataBindToTarget->addDirt(ComponentDirt::Bindings, true);
    }
}