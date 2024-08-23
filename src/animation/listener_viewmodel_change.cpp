#include "rive/animation/listener_viewmodel_change.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/data_bind/bindable_property.hpp"
#include "rive/importers/bindable_property_importer.hpp"

using namespace rive;

StatusCode ListenerViewModelChange::import(ImportStack& importStack)
{

    auto bindablePropertyImporter =
        importStack.latest<BindablePropertyImporter>(BindablePropertyBase::typeKey);
    if (bindablePropertyImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    m_bindableProperty = bindablePropertyImporter->bindableProperty();

    return Super::import(importStack);
}

// Note: perform works the same way whether the value comes from a direct value assignment or from
// another view model. In the case of coming from another view model, the state machine instance
// method "updataDataBinds" will handle updating the value of the bound object. That's the benefit
// of binding the same bindable property with two data binding objects.
void ListenerViewModelChange::perform(StateMachineInstance* stateMachineInstance,
                                      Vec2D position,
                                      Vec2D previousPosition) const
{
    // Get the bindable property instance from the state machine instance context
    auto bindableInstance = stateMachineInstance->bindablePropertyInstance(m_bindableProperty);
    // Get the data bound object (that goes from target to source) from this bindable instance
    auto dataBind = stateMachineInstance->bindableDataBind(bindableInstance);
    // Apply the change that will assign the value of the bindable property to the view model
    // property instance
    dataBind->updateSourceBinding();
}