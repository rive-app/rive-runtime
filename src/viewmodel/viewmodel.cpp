#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"

using namespace rive;

void ViewModel::addProperty(ViewModelProperty* property) { m_Properties.push_back(property); }

ViewModelProperty* ViewModel::property(size_t index)
{
    if (index < m_Properties.size())
    {
        return m_Properties[index];
    }
    return nullptr;
}

ViewModelProperty* ViewModel::property(const std::string& name)
{
    for (auto property : m_Properties)
    {
        if (property->name() == name)
        {
            return property;
        }
    }
    return nullptr;
}

void ViewModel::addInstance(ViewModelInstance* value)
{
    m_Instances.push_back(value);
    value->viewModel(this);
}

ViewModelInstance* ViewModel::defaultInstance() { return m_Instances[defaultInstanceId()]; }

ViewModelInstance* ViewModel::instance(size_t index)
{
    if (index < m_Instances.size())
    {
        return m_Instances[index];
    }
    return nullptr;
}

ViewModelInstance* ViewModel::instance(const std::string& name)
{
    for (auto instance : m_Instances)
    {
        if (instance->name() == name)
        {
            return instance;
        }
    }
    return nullptr;
}