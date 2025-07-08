
#include "rive/viewmodel/runtime/viewmodel_instance_list_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_runtime.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

ViewModelInstanceListRuntime::~ViewModelInstanceListRuntime()
{
    for (auto& list : m_itemsMap)
    {
        list.first->unref();
        list.second->unref();
    }
}

ViewModelInstanceRuntime* ViewModelInstanceListRuntime::instanceAt(int index)
{
    auto listItems =
        m_viewModelInstanceValue->as<ViewModelInstanceList>()->listItems();
    if (index >= listItems.size() || index < 0)
    {
        return nullptr;
    }
    auto listItem = listItems[index];
    if (listItem->viewModelInstance() == nullptr)
    {
        return nullptr;
    }
    auto it = m_itemsMap.find(listItem.get());
    if (it != m_itemsMap.end())
    {
        return it->second;
    }
    auto instanceRuntime =
        new ViewModelInstanceRuntime(listItem->viewModelInstance());
    m_itemsMap[listItem.get()] = instanceRuntime;
    return instanceRuntime;
}

void ViewModelInstanceListRuntime::addInstance(
    ViewModelInstanceRuntime* instanceRuntime)
{
    instanceRuntime->ref();
    auto listItem = new ViewModelInstanceListItem();
    listItem->viewModelInstance(instanceRuntime->instance());
    auto list = m_viewModelInstanceValue->as<ViewModelInstanceList>();
    m_itemsMap[listItem] = instanceRuntime;
    list->addItem(rcp<ViewModelInstanceListItem>(listItem));
}

bool ViewModelInstanceListRuntime::addInstanceAt(
    ViewModelInstanceRuntime* instanceRuntime,
    int index)
{
    auto listItem = make_rcp<ViewModelInstanceListItem>();
    auto list = m_viewModelInstanceValue->as<ViewModelInstanceList>();
    if (list->addItemAt(listItem, index))
    {
        instanceRuntime->ref();
        listItem->viewModelInstance(instanceRuntime->instance());
        m_itemsMap[listItem.get()] = instanceRuntime;
        return true;
    }
    return false;
}

void ViewModelInstanceListRuntime::removeInstance(
    ViewModelInstanceRuntime* instanceRuntime)
{
    auto instanceList = m_viewModelInstanceValue->as<ViewModelInstanceList>();
    auto listItems = instanceList->listItems();
    // The same instance might be present multiple times in the list
    std::vector<rcp<ViewModelInstanceListItem>> itemsToRemove;
    for (auto& item : listItems)
    {
        if (item->viewModelInstance().get() ==
            instanceRuntime->instance().get())
        {
            itemsToRemove.push_back(item);
        }
    }
    for (auto& item : itemsToRemove)
    {
        auto it = m_itemsMap.find(item.get());
        if (it != m_itemsMap.end())
        {
            it->first->unref();
            it->second->unref();
            m_itemsMap.erase(it);
        }
        instanceList->removeItem(item);
    }
}

void ViewModelInstanceListRuntime::removeInstanceAt(int index)
{
    auto instanceList = m_viewModelInstanceValue->as<ViewModelInstanceList>();
    auto listItems = instanceList->listItems();
    if (index >= 0 && index < listItems.size())
    {
        auto listItem = listItems[index];
        instanceList->removeItem(listItem);

        auto it = m_itemsMap.find(listItem.get());
        if (it != m_itemsMap.end())
        {
            it->first->unref();
            it->second->unref();
            m_itemsMap.erase(it);
        }
    }
}

void ViewModelInstanceListRuntime::swap(uint32_t a, uint32_t b)
{
    auto instanceList = m_viewModelInstanceValue->as<ViewModelInstanceList>();
    instanceList->swap(a, b);
}

size_t ViewModelInstanceListRuntime::size() const
{
    auto listItems =
        m_viewModelInstanceValue->as<ViewModelInstanceList>()->listItems();
    return listItems.size();
}
