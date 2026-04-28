#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/data_bind/data_values/data_value_viewmodel.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_property_viewmodel.hpp"
#include "rive/refcnt.hpp"
#include "rive/backboard.hpp"
#include "rive/file.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/viewmodel_instance_importer.hpp"

using namespace rive;

void ViewModelInstanceViewModel::propertyValueChanged()
{
    addDirt(ComponentDirt::Bindings);
#ifdef WITH_RIVE_TOOLS
    if (m_changedCallback != nullptr)
    {
        m_changedCallback(this);
    }
#endif
    onValueChanged();
}

void ViewModelInstanceViewModel::setRoot(rcp<ViewModelInstance> value)
{
    Super::setRoot(value);
    if (m_referenceViewModelInstance)
    {
        m_referenceViewModelInstance->setRoot(value);
    }
}

void ViewModelInstanceViewModel::advanced()
{
    if (m_referenceViewModelInstance != nullptr)
    {
        m_referenceViewModelInstance->advanced();
    }
}

StatusCode ViewModelInstanceViewModel::import(ImportStack& importStack)
{
    auto status = Super::import(importStack);
    auto artboardImporter =
        importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
    if (artboardImporter != nullptr)
    {

        auto backboardImporter =
            importStack.latest<BackboardImporter>(Backboard::typeKey);
        if (backboardImporter == nullptr)
        {
            return StatusCode::MissingObject;
        }
        auto file = backboardImporter->file();
        // Look for the referenced view model instance of this property
        // In order to find it we need to go through some steps:
        // - find the view model instance this property belongs to (O(1))
        // - find the view model from which its an instance of (O(1))
        // - find the view model property from which this is an instance of
        // (O(1))
        // - find the view model the property is referencing to (O(1))
        // - find the specific instance this instance value is pointing to
        // (O(1))
        if (file)
        {
            auto viewModelInstanceImporter =
                importStack.latest<ViewModelInstanceImporter>(
                    ViewModelInstance::typeKey);
            auto viewModelInstance =
                viewModelInstanceImporter->viewModelInstance();
            if (viewModelInstance)
            {
                auto viewModel =
                    file->viewModel(viewModelInstance->viewModelId());
                if (viewModel)
                {
                    auto viewModelProperty =
                        viewModel->property(viewModelPropertyId());
                    if (viewModelProperty != nullptr &&
                        viewModelProperty->is<ViewModelPropertyViewModel>())
                    {
                        auto viewModelPropertyViewModel =
                            viewModelProperty->as<ViewModelPropertyViewModel>();
                        auto referencedViewModel = file->viewModel(
                            viewModelPropertyViewModel->viewModelReferenceId());
                        if (referencedViewModel)
                        {
                            auto instance =
                                referencedViewModel->instance(propertyValue());
                            if (instance)
                            {
                                referenceViewModelInstance(ref_rcp(instance));
                            }
                        }
                    }
                }
            }
        }
    }

    return status;
}

void ViewModelInstanceViewModel::updateViewModel(ViewModelInstance* value)
{
    m_viewModelInstance->replaceViewModelByProperty(this, ref_rcp(value));
    auto dependentsSnapshot = dependents();
    for (auto& dependent : dependentsSnapshot)
    {
        dependent->relinkDataBind();
    }
}

void ViewModelInstanceViewModel::applyValue(DataValueViewModel* dataValue)
{
    updateViewModel(dataValue->value());
}

Core* ViewModelInstanceViewModel::clone() const
{
    ViewModelInstanceViewModel* cloned = ViewModelInstanceViewModelBase::clone()
                                             ->as<ViewModelInstanceViewModel>();
    if (m_referenceViewModelInstance)
    {
        auto clonedInstance = m_referenceViewModelInstance->clone();
        cloned->referenceViewModelInstance(
            rcp<ViewModelInstance>(clonedInstance->as<ViewModelInstance>()));
        cloned->viewModelInstance(m_viewModelInstance);
    }
    return cloned;
}