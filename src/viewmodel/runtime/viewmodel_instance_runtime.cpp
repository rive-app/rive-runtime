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
#include "rive/viewmodel/runtime/viewmodel_instance_list_runtime.hpp"
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
}

const std::string& ViewModelInstanceRuntime::name() const
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
        auto instance = instanceRuntime(viewModelInstanceName);
        if (instance != nullptr)
        {
            if (restOfPath.empty())
            {
                return instance.get();
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
    auto viewModelInstanceRuntime = viewModelInstanceFromFullPath(path);
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
                    case DataType::color:
                        return viewModelInstanceRuntime->propertyColor(
                            propertyName);
                    case DataType::assetImage:
                        return viewModelInstanceRuntime->propertyImage(
                            propertyName);
                    case DataType::list:
                        return viewModelInstanceRuntime->propertyList(
                            propertyName);
                    case DataType::enumType:
                        return viewModelInstanceRuntime->propertyEnum(
                            propertyName);
                    case DataType::trigger:
                        return viewModelInstanceRuntime->propertyTrigger(
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
    viewModelInstanceFromFullPath(const std::string& path) const
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
    auto viewModelInstance = viewModelInstanceFromFullPath(path);
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
    auto viewModelInstance = viewModelInstanceFromFullPath(path);
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
    auto viewModelInstance = viewModelInstanceFromFullPath(path);
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
    auto viewModelInstance = viewModelInstanceFromFullPath(path);
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
    auto viewModelInstance = viewModelInstanceFromFullPath(path);
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
    auto viewModelInstance = viewModelInstanceFromFullPath(path);
    if (viewModelInstance != nullptr)
    {

        return viewModelInstance
            ->getPropertyInstance<ViewModelInstanceEnum,
                                  ViewModelInstanceEnumRuntime>(propertyName);
    }
    return nullptr;
}

ViewModelInstanceListRuntime* ViewModelInstanceRuntime::propertyList(
    const std::string& path) const
{
    const auto propertyName = getPropertyNameFromPath(path);
    auto viewModelInstance = viewModelInstanceFromFullPath(path);
    if (viewModelInstance != nullptr)
    {

        return viewModelInstance
            ->getPropertyInstance<ViewModelInstanceList,
                                  ViewModelInstanceListRuntime>(propertyName);
    }
    return nullptr;
}

rcp<ViewModelInstance> ViewModelInstanceRuntime::viewModelInstanceProperty(
    const std::string& name) const
{
    auto viewModelInstanceValue = m_viewModelInstance->propertyValue(name);
    if (viewModelInstanceValue != nullptr &&
        viewModelInstanceValue->is<ViewModelInstanceViewModel>())
    {
        return viewModelInstanceValue->as<ViewModelInstanceViewModel>()
            ->referenceViewModelInstance();
    }
    return nullptr;
}

rcp<ViewModelInstanceRuntime> ViewModelInstanceRuntime::instanceRuntime(
    const std::string& name) const
{
    auto itr = m_viewModelInstances.find(name);
    if (itr != m_viewModelInstances.end())
    {
        return itr->second;
    }
    auto viewModelInstance = viewModelInstanceProperty(name);
    if (viewModelInstance != nullptr)
    {
        auto viewModelInstanceRef =
            make_rcp<ViewModelInstanceRuntime>(viewModelInstance);
        m_viewModelInstances[name] = viewModelInstanceRef;
        return viewModelInstanceRef;
    }
    return nullptr;
}

ViewModelInstanceRuntime* ViewModelInstanceRuntime::propertyViewModel(
    const std::string& path) const
{
    const auto propertyName = getPropertyNameFromPath(path);
    auto viewModelInstance = viewModelInstanceFromFullPath(path);
    if (viewModelInstance != nullptr)
    {
        return viewModelInstance->instanceRuntime(propertyName).get();
    }
    return nullptr;
}

ViewModelInstanceAssetImageRuntime* ViewModelInstanceRuntime::propertyImage(
    const std::string& path) const
{
    const auto propertyName = getPropertyNameFromPath(path);
    auto viewModelInstance = viewModelInstanceFromFullPath(path);
    if (viewModelInstance != nullptr)
    {

        return viewModelInstance
            ->getPropertyInstance<ViewModelInstanceAssetImage,
                                  ViewModelInstanceAssetImageRuntime>(
                propertyName);
    }
    return nullptr;
}

bool ViewModelInstanceRuntime::replaceViewModel(
    const std::string& path,
    ViewModelInstanceRuntime* value) const
{
    const auto propertyName = getPropertyNameFromPath(path);
    auto viewModelInstance = viewModelInstanceFromFullPath(path);
    if (viewModelInstance != nullptr)
    {
        return viewModelInstance->replaceViewModelByName(propertyName, value);
    }
    return false;
}

bool ViewModelInstanceRuntime::replaceViewModelByName(
    const std::string& name,
    ViewModelInstanceRuntime* value) const
{
    if (m_viewModelInstance->replaceViewModelByName(name, value->instance()))
    {
        bool isStored = false;
        for (const auto& rcpInstance : m_viewModelInstances)
        {
            if (rcpInstance.second.get() == value)
            {
                isStored = true;
                break;
            }
        }
        if (!isStored)
        {
            m_viewModelInstances[name] = ref_rcp(value);
        }
        return true;
    }
    return false;
}

std::vector<PropertyData> ViewModelInstanceRuntime::properties() const
{
    std::vector<PropertyData> props;
    auto properties = m_viewModelInstance->viewModel()->properties();
    return ViewModelRuntime::buildPropertiesData(properties);
}