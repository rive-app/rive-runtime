#include "rive/animation/blend_state_1d_viewmodel.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/importers/bindable_property_importer.hpp"

using namespace rive;

BlendState1DViewModel::~BlendState1DViewModel()
{
    if (m_bindableProperty != nullptr)
    {
        delete m_bindableProperty;
        m_bindableProperty = nullptr;
    }
}

StatusCode BlendState1DViewModel::import(ImportStack& importStack)
{
    auto stateMachineImporter =
        importStack.latest<StateMachineImporter>(StateMachine::typeKey);
    if (stateMachineImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
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