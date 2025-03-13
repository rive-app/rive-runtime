
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_property_string.hpp"
#include "rive/viewmodel/viewmodel_property_number.hpp"
#include "rive/viewmodel/viewmodel_property_boolean.hpp"
#include "rive/viewmodel/viewmodel_property_color.hpp"
#include "rive/viewmodel/viewmodel_property_list.hpp"
#include "rive/viewmodel/viewmodel_property_enum.hpp"
#include "rive/viewmodel/viewmodel_property_enum_custom.hpp"
#include "rive/viewmodel/viewmodel_property_enum_system.hpp"
#include "rive/viewmodel/viewmodel_property_trigger.hpp"
#include "rive/viewmodel/viewmodel_property_viewmodel.hpp"
#include "rive/file.hpp"
#include "rive/refcnt.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

ViewModelRuntime::ViewModelRuntime(ViewModel* viewModel, const File* file) :
    m_viewModel(viewModel), m_file(file)
{}

size_t ViewModelRuntime::instanceCount() const
{
    return m_viewModel->instanceCount();
}

size_t ViewModelRuntime::propertyCount() const
{
    return m_viewModel->properties().size();
}

const std::string& ViewModelRuntime::name() const
{
    return m_viewModel->name();
}

std::vector<PropertyData> ViewModelRuntime::buildPropertiesData(
    std::vector<rive::ViewModelProperty*>& properties)
{
    std::vector<PropertyData> props;
    for (auto property : properties)
    {
        DataType type = DataType::none;
        switch (property->coreType())
        {
            case ViewModelPropertyString::typeKey:
                type = DataType::string;
                break;
            case ViewModelPropertyNumber::typeKey:
                type = DataType::number;
                break;
            case ViewModelPropertyBoolean::typeKey:
                type = DataType::boolean;
                break;
            case ViewModelPropertyColor::typeKey:
                type = DataType::color;
                break;
            case ViewModelPropertyList::typeKey:
                type = DataType::list;
                break;
            case ViewModelPropertyEnum::typeKey:
            case ViewModelPropertyEnumCustomBase::typeKey:
            case ViewModelPropertyEnumSystemBase::typeKey:
                type = DataType::enumType;
                break;
            case ViewModelPropertyTrigger::typeKey:
                type = DataType::trigger;
                break;
            case ViewModelPropertyViewModelBase::typeKey:
                type = DataType::viewModel;
                break;
            default:
                break;
        }
        props.push_back({type, property->name()});
    }
    return props;
}

std::vector<PropertyData> ViewModelRuntime::properties()
{
    auto props = m_viewModel->properties();
    return buildPropertiesData(props);
}

std::vector<std::string> ViewModelRuntime::instanceNames() const
{
    auto instances = m_viewModel->instances();
    std::vector<std::string> names;
    for (auto instance : instances)
    {
        names.push_back(instance->name());
    }
    return names;
}

ViewModelInstanceRuntime* ViewModelRuntime::createInstanceFromIndex(
    const size_t index) const
{
    auto viewModelInstance = m_viewModel->instance(index);
    if (viewModelInstance != nullptr)
    {
        auto copy = rcp<ViewModelInstance>(
            viewModelInstance->clone()->as<ViewModelInstance>());
        m_file->completeViewModelInstance(copy);
        return createRuntimeInstance(copy);
    }
    return nullptr;
}

ViewModelInstanceRuntime* ViewModelRuntime::createInstanceFromName(
    const std::string& name) const
{
    auto viewModelInstance = m_viewModel->instance(name);
    if (viewModelInstance != nullptr)
    {
        auto copy = rcp<ViewModelInstance>(
            viewModelInstance->clone()->as<ViewModelInstance>());
        m_file->completeViewModelInstance(copy);
        return createRuntimeInstance(copy);
    }
    return nullptr;
}

ViewModelInstanceRuntime* ViewModelRuntime::createDefaultInstance() const
{
    auto viewModelInstance =
        m_file->createDefaultViewModelInstance(m_viewModel);
    if (viewModelInstance != nullptr)
    {
        return createRuntimeInstance(viewModelInstance);
    }
    return createInstance();
}

ViewModelInstanceRuntime* ViewModelRuntime::createInstance() const
{
    auto instance = m_file->createViewModelInstance(m_viewModel);
    return createRuntimeInstance(instance);
}

ViewModelInstanceRuntime* ViewModelRuntime::createRuntimeInstance(
    rcp<ViewModelInstance> instance) const
{
    if (instance != nullptr)
    {
        auto viewModelInstanceRuntime = rcp<ViewModelInstanceRuntime>(
            new ViewModelInstanceRuntime(instance));
        m_viewModelInstanceRuntimes.push_back(viewModelInstanceRuntime);
        return viewModelInstanceRuntime.get();
    }
    return nullptr;
}