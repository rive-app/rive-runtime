#include "rive/data_bind/data_context.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"

using namespace rive;

DataContext::DataContext(ViewModelInstance* viewModelInstance) :
    m_ViewModelInstance(viewModelInstance)
{}

void DataContext::viewModelInstance(ViewModelInstance* value)
{
    m_ViewModelInstance = value;
}

ViewModelInstanceValue* DataContext::getViewModelProperty(
    const std::vector<uint32_t> path) const
{
    std::vector<uint32_t>::const_iterator it;
    if (path.size() == 0)
    {
        return nullptr;
    }
    if (m_ViewModelInstance != nullptr &&
        m_ViewModelInstance->viewModelId() == path[0])
    {
        ViewModelInstance* instance = m_ViewModelInstance;
        for (it = path.begin() + 1; it != path.end() - 1; it++)
        {
            auto viewModelInstanceValue = instance->propertyValue(*it);
            if (viewModelInstanceValue != nullptr &&
                viewModelInstanceValue->is<ViewModelInstanceViewModel>())
            {
                instance = viewModelInstanceValue->as<ViewModelInstanceViewModel>()
                               ->referenceViewModelInstance();
            }
            else
            {
                goto skip_path;
            }
        }
        ViewModelInstanceValue* value = instance->propertyValue(*it++);
        return value;
    }
skip_path:
    if (m_Parent != nullptr)
    {
        return m_Parent->getViewModelProperty(path);
    }
    return nullptr;
}

ViewModelInstance* DataContext::getViewModelInstance(
    const std::vector<uint32_t> path) const
{
    std::vector<uint32_t>::const_iterator it;
    if (path.size() == 0)
    {
        return nullptr;
    }
    if (m_ViewModelInstance != nullptr &&
        m_ViewModelInstance->viewModelId() == path[0])
    {
        ViewModelInstance* instance = m_ViewModelInstance;
        for (it = path.begin() + 1; it != path.end(); it++)
        {
            auto viewModelInstanceValue = instance->propertyValue(*it);
            if (viewModelInstanceValue != nullptr &&
                viewModelInstanceValue->is<ViewModelInstanceViewModel>())
            {
                instance = viewModelInstanceValue->as<ViewModelInstanceViewModel>()
                               ->referenceViewModelInstance();
            }
            else
            {
                instance = nullptr;
            }
            if (instance == nullptr)
            {
                goto skip_path;
            }
        }
        return instance;
    }
skip_path:
    if (m_Parent != nullptr)
    {
        return m_Parent->getViewModelInstance(path);
    }
    return nullptr;
}