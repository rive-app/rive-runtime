#include "rive/animation/listener_viewmodel_change.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/data_bind/bindable_property.hpp"
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

// Note: perform works the same way whether the value comes from a direct value
// assignment or from another view model. In the case of coming from another
// view model, the state machine instance method "updataDataBinds" will handle
// updating the value of the bound object. That's the benefit of binding the
// same bindable property with two data binding objects.
void ListenerViewModelChange::perform(
    StateMachineInstance* stateMachineInstance,
    Vec2D position,
    Vec2D previousPosition,
    int pointerId) const
{
    // Get the bindable property instance from the state machine instance
    // context
    auto bindableInstance =
        stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
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
                auto value = context->viewModelInstance().get();
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