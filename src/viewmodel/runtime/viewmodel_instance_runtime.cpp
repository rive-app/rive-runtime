
#include "rive/viewmodel/runtime/viewmodel_instance_runtime.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_number_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_string_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_boolean_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_color_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_enum_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_trigger_runtime.hpp"
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
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

ViewModelInstanceRuntime::ViewModelInstanceRuntime(
    rcp<ViewModelInstance> instance) :
    m_viewModelInstance(instance)
{}

ViewModelInstanceRuntime::~ViewModelInstanceRuntime()
{
    for (auto& it : m_properties)
    {
        delete it.second;
    }
    for (auto& it : m_viewModelInstances)
    {
        delete it.second;
    }
}

std::string ViewModelInstanceRuntime::name() const
{
    return m_viewModelInstance->name();
}

size_t ViewModelInstanceRuntime::propertyCount() const
{
    return m_viewModelInstance->propertyValues().size();
}

ViewModelInstanceRuntime* ViewModelInstanceRuntime::viewModelInstanceAtPath(
    const std::string& path) const
{
    std::string delimiter = "/";
    size_t firstDelim = path.find(delimiter);
    std::string viewModelInstanceName =
        firstDelim == std::string::npos ? path : path.substr(0, firstDelim);
    std::string restOfPath = firstDelim == std::string::npos
                                 ? ""
                                 : path.substr(firstDelim + 1, path.size());

    if (!viewModelInstanceName.empty())
    {
        auto instance = viewModelInstanceProperty(viewModelInstanceName);
        if (instance != nullptr)
        {
            if (restOfPath.empty())
            {
                return instance;
            }
            else
            {
                return instance->viewModelInstanceAtPath(restOfPath);
            }
        }
    }
    return nullptr;
}

ViewModelInstanceValueRuntime* ViewModelInstanceRuntime::property(
    const std::string& path) const
{
    if (path.empty())
    {
        return nullptr;
    }
    const auto propertyName = getPropertyNameFromPath(path);
    auto viewModelInstanceRuntime = getViewModelInstanceFromPath(path);
    if (viewModelInstanceRuntime != nullptr)
    {
        auto properties = viewModelInstanceRuntime->properties();
        for (auto p : properties)
        {
            if (p.name == propertyName)
            {
                switch (p.type)
                {
                    case DataType::string:
                        return viewModelInstanceRuntime->propertyString(
                            propertyName);
                    case DataType::number:
                        return viewModelInstanceRuntime->propertyNumber(
                            propertyName);
                    case DataType::boolean:
                        return viewModelInstanceRuntime->propertyBoolean(
                            propertyName);
                    default:
                        break;
                }
            }
        }
    }
    return nullptr;
}

std::string ViewModelInstanceRuntime::getPropertyNameFromPath(
    const std::string& path) const
{
    if (!path.empty())
    {
        std::string delimiter = "/";
        auto propertyNameDelimiter = path.rfind(delimiter);
        return propertyNameDelimiter == std::string::npos
                   ? path
                   : path.substr(propertyNameDelimiter + 1);
    }
    return "";
}

const ViewModelInstanceRuntime* ViewModelInstanceRuntime::
    getViewModelInstanceFromPath(const std::string& path) const
{
    std::string delimiter = "/";
    auto propertyNameDelimiter = path.rfind(delimiter);
    if (propertyNameDelimiter != std::string::npos)
    {
        return viewModelInstanceAtPath(path.substr(0, propertyNameDelimiter));
    }
    return this;
}

ViewModelInstanceNumberRuntime* ViewModelInstanceRuntime::propertyNumber(
    const std::string& path) const
{
    const auto propertyName = getPropertyNameFromPath(path);
    auto viewModelInstance = getViewModelInstanceFromPath(path);
    if (viewModelInstance != nullptr)
    {

        return viewModelInstance
            ->getPropertyInstance<ViewModelInstanceNumber,
                                  ViewModelInstanceNumberRuntime>(propertyName);
    }
    return nullptr;
}

ViewModelInstanceBooleanRuntime* ViewModelInstanceRuntime::propertyBoolean(
    const std::string& path) const
{
    const auto propertyName = getPropertyNameFromPath(path);
    auto viewModelInstance = getViewModelInstanceFromPath(path);
    if (viewModelInstance != nullptr)
    {

        return viewModelInstance
            ->getPropertyInstance<ViewModelInstanceBoolean,
                                  ViewModelInstanceBooleanRuntime>(
                propertyName);
    }
    return nullptr;
}

ViewModelInstanceStringRuntime* ViewModelInstanceRuntime::propertyString(
    const std::string& path) const
{
    const auto propertyName = getPropertyNameFromPath(path);
    auto viewModelInstance = getViewModelInstanceFromPath(path);
    if (viewModelInstance != nullptr)
    {

        return viewModelInstance
            ->getPropertyInstance<ViewModelInstanceString,
                                  ViewModelInstanceStringRuntime>(propertyName);
    }
    return nullptr;
}

ViewModelInstanceColorRuntime* ViewModelInstanceRuntime::propertyColor(
    const std::string& path) const
{
    const auto propertyName = getPropertyNameFromPath(path);
    auto viewModelInstance = getViewModelInstanceFromPath(path);
    if (viewModelInstance != nullptr)
    {

        return viewModelInstance
            ->getPropertyInstance<ViewModelInstanceColor,
                                  ViewModelInstanceColorRuntime>(propertyName);
    }
    return nullptr;
}

ViewModelInstanceTriggerRuntime* ViewModelInstanceRuntime::propertyTrigger(
    const std::string& path) const
{
    const auto propertyName = getPropertyNameFromPath(path);
    auto viewModelInstance = getViewModelInstanceFromPath(path);
    if (viewModelInstance != nullptr)
    {

        return viewModelInstance
            ->getPropertyInstance<ViewModelInstanceTrigger,
                                  ViewModelInstanceTriggerRuntime>(
                propertyName);
    }
    return nullptr;
}

ViewModelInstanceEnumRuntime* ViewModelInstanceRuntime::propertyEnum(
    const std::string& path) const
{
    const auto propertyName = getPropertyNameFromPath(path);
    auto viewModelInstance = getViewModelInstanceFromPath(path);
    if (viewModelInstance != nullptr)
    {

        return viewModelInstance
            ->getPropertyInstance<ViewModelInstanceEnum,
                                  ViewModelInstanceEnumRuntime>(propertyName);
    }
    return nullptr;
}

ViewModelInstanceRuntime* ViewModelInstanceRuntime::viewModelInstanceProperty(
    const std::string& name) const
{
    auto itr = m_viewModelInstances.find(name);
    if (itr != m_viewModelInstances.end())
    {
        return static_cast<ViewModelInstanceRuntime*>(itr->second);
    }
    auto viewModelInstanceValue = m_viewModelInstance->propertyValue(name);
    if (viewModelInstanceValue != nullptr &&
        viewModelInstanceValue->is<ViewModelInstanceViewModel>())
    {
        auto runtimeInstance = new ViewModelInstanceRuntime(
            viewModelInstanceValue->as<ViewModelInstanceViewModel>()
                ->referenceViewModelInstance());
        m_viewModelInstances[name] = runtimeInstance;
        return runtimeInstance;
    }
    return nullptr;
}

ViewModelInstanceRuntime* ViewModelInstanceRuntime::propertyViewModel(
    const std::string& path) const
{
    const auto propertyName = getPropertyNameFromPath(path);
    auto viewModelInstance = getViewModelInstanceFromPath(path);
    if (viewModelInstance != nullptr)
    {
        return viewModelInstance->viewModelInstanceProperty(propertyName);
    }
    return nullptr;
}

std::vector<PropertyData> ViewModelInstanceRuntime::properties() const
{
    std::vector<PropertyData> props;
    auto properties = m_viewModelInstance->viewModel()->properties();
    return ViewModelRuntime::buildPropertiesData(properties);
}