#include "rive/data_bind/data_context.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"

using namespace rive;

DataContext::DataContext(ViewModelInstance* viewModelInstance) :
    m_ViewModelInstance(viewModelInstance)
{}

DataContext::DataContext() : m_ViewModelInstances({}) {}

DataContext::~DataContext() {}

void DataContext::addViewModelInstance(ViewModelInstance* value)
{
    m_ViewModelInstances.push_back(value);
}

void DataContext::viewModelInstance(ViewModelInstance* value) { m_ViewModelInstance = value; }

ViewModelInstanceValue* DataContext::getViewModelProperty(const std::vector<uint32_t> path) const
{
    std::vector<uint32_t>::const_iterator it;
    if (path.size() == 0)
    {
        return nullptr;
    }
    // TODO: @hernan review. We should probably remove the std::vector and only keep the instance
    for (auto viewModel : m_ViewModelInstances)
    {
        if (viewModel->viewModelId() == path[0])
        {
            ViewModelInstance* instance = viewModel;
            for (it = path.begin() + 1; it != path.end() - 1; it++)
            {
                instance = instance->propertyValue(*it)
                               ->as<ViewModelInstanceViewModel>()
                               ->referenceViewModelInstance();
            }
            ViewModelInstanceValue* value = instance->propertyValue(*it++);
            return value;
        }
    }
    if (m_ViewModelInstance != nullptr && m_ViewModelInstance->viewModelId() == path[0])
    {
        ViewModelInstance* instance = m_ViewModelInstance;
        for (it = path.begin() + 1; it != path.end() - 1; it++)
        {
            instance = instance->propertyValue(*it)
                           ->as<ViewModelInstanceViewModel>()
                           ->referenceViewModelInstance();
        }
        ViewModelInstanceValue* value = instance->propertyValue(*it++);
        return value;
    }
    if (m_Parent != nullptr)
    {
        return m_Parent->getViewModelProperty(path);
    }
    return nullptr;
}

ViewModelInstance* DataContext::getViewModelInstance(const std::vector<uint32_t> path) const
{
    std::vector<uint32_t>::const_iterator it;
    if (path.size() == 0)
    {
        return nullptr;
    }
    if (m_ViewModelInstance != nullptr && m_ViewModelInstance->viewModelId() == path[0])
    {
        ViewModelInstance* instance = m_ViewModelInstance;
        for (it = path.begin() + 1; it != path.end(); it++)
        {
            instance = instance->propertyValue(*it)
                           ->as<ViewModelInstanceViewModel>()
                           ->referenceViewModelInstance();
            if (instance == nullptr)
            {
                return instance;
            }
        }
        return instance;
    }
    if (m_Parent != nullptr)
    {
        return m_Parent->getViewModelInstance(path);
    }
    return nullptr;
}