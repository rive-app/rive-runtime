#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/component_dirt.hpp"

using namespace rive;

ViewModelInstanceList::~ViewModelInstanceList()
{
    if (parentViewModelInstance())
    {
        for (auto& item : m_ListItems)
        {
            if (item->viewModelInstance() != nullptr)
            {
                item->viewModelInstance()->removeParent(
                    parentViewModelInstance());
            }
        }
    }
}

void ViewModelInstanceList::propertyValueChanged()
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

void ViewModelInstanceList::addItem(rcp<ViewModelInstanceListItem> item)
{
    m_ListItems.push_back(item);
    if (item->viewModelInstance())
    {
        item->viewModelInstance()->addParent(parentViewModelInstance());
    }
    propertyValueChanged();
}

bool ViewModelInstanceList::addItemAt(rcp<ViewModelInstanceListItem> item,
                                      int index)
{
    if (index >= 0 && index <= m_ListItems.size())
    {
        m_ListItems.insert(m_ListItems.begin() + index, item);

        if (item->viewModelInstance())
        {
            item->viewModelInstance()->addParent(parentViewModelInstance());
        }
        propertyValueChanged();
        return true;
    }
    return false;
}

void ViewModelInstanceList::internalAddItem(rcp<ViewModelInstanceListItem> item)
{
    // For ViewModelInstanceListItems that are built as a core object
    // we skip the ref since core has already reffed it
    m_ListItems.push_back(item);
    if (item->viewModelInstance())
    {
        item->viewModelInstance()->addParent(parentViewModelInstance());
    }
}

void ViewModelInstanceList::removeItem(int index)
{
    // TODO: @hernan decide if we want to return a boolean
    if (index >= 0 && index < m_ListItems.size())
    {
        auto listItem = m_ListItems[index];
        if (listItem->viewModelInstance())
        {
            listItem->viewModelInstance()->removeParent(
                parentViewModelInstance());
        }
        m_ListItems.erase(m_ListItems.begin() + index);
        propertyValueChanged();
    }
}

rcp<ViewModelInstanceListItem> ViewModelInstanceList::pop()
{
    if (m_ListItems.size() > 0)
    {

        auto listItem = m_ListItems.back();
        m_ListItems.pop_back();
        propertyValueChanged();
        return listItem;
    }
    return nullptr;
}

rcp<ViewModelInstanceListItem> ViewModelInstanceList::shift()
{
    if (m_ListItems.size() > 0)
    {

        auto listItem = m_ListItems.front();
        removeItem(0);
        return listItem;
    }
    return nullptr;
}

void ViewModelInstanceList::removeItem(rcp<ViewModelInstanceListItem> listItem)
{
    auto noSpaceEnd =
        std::remove(m_ListItems.begin(), m_ListItems.end(), listItem);
    m_ListItems.erase(noSpaceEnd, m_ListItems.end());
    if (listItem->viewModelInstance())
    {
        listItem->viewModelInstance()->removeParent(parentViewModelInstance());
    }
    propertyValueChanged();
}

void ViewModelInstanceList::removeAllItems()
{
    if (m_ListItems.size() > 0)
    {
        for (auto& listItem : m_ListItems)
        {
            if (listItem->viewModelInstance())
            {
                listItem->viewModelInstance()->removeParent(
                    parentViewModelInstance());
            }
        }
        m_ListItems.clear();
        propertyValueChanged();
    }
}

void ViewModelInstanceList::removeAllItemsWithViewModelInstance(
    ViewModelInstance* viewModelInstance)
{
    if (viewModelInstance == nullptr || m_ListItems.empty())
    {
        return;
    }

    bool changed = false;
    for (auto it = m_ListItems.begin(); it != m_ListItems.end();)
    {
        if ((*it)->viewModelInstance().get() == viewModelInstance)
        {
            (*it)->viewModelInstance()->removeParent(parentViewModelInstance());
            it = m_ListItems.erase(it);
            changed = true;
        }
        else
        {
            ++it;
        }
    }
    if (changed)
    {
        propertyValueChanged();
    }
}

rcp<ViewModelInstanceListItem> ViewModelInstanceList::item(uint32_t index)
{
    if (index < m_ListItems.size())
    {
        return m_ListItems[index];
    }
    return nullptr;
}

void ViewModelInstanceList::swap(uint32_t index1, uint32_t index2)
{
    if (index1 < m_ListItems.size() && index2 < m_ListItems.size())
    {
        std::iter_swap(m_ListItems.begin() + index1,
                       m_ListItems.begin() + index2);
        propertyValueChanged();
    }
}

void ViewModelInstanceList::updateList(
    std::vector<rcp<ViewModelInstanceListItem>>* list)
{
    if (list == nullptr)
    {
        return;
    }

    // Detach old children from this host instance before replacing the list.
    if (parentViewModelInstance())
    {
        for (auto& item : m_ListItems)
        {
            if (item->viewModelInstance() != nullptr)
            {
                item->viewModelInstance()->removeParent(
                    parentViewModelInstance());
            }
        }
    }

    m_ListItems.clear();
    m_ListItems.reserve(list->size());
    for (auto& item : *list)
    {
        m_ListItems.push_back(item);
        if (parentViewModelInstance() && item->viewModelInstance() != nullptr)
        {
            item->viewModelInstance()->addParent(parentViewModelInstance());
        }
    }
    propertyValueChanged();
}

Core* ViewModelInstanceList::clone() const
{
    auto cloned = new ViewModelInstanceList();
    cloned->copy(*this);
    for (auto property : m_ListItems)
    {
        auto clonedValue = property->clone()->as<ViewModelInstanceListItem>();
        cloned->internalAddItem(rcp<ViewModelInstanceListItem>(clonedValue));
    }
    return cloned;
}

void ViewModelInstanceList::advanced()
{
    for (auto item : m_ListItems)
    {
        if (item->viewModelInstance() != nullptr)
        {
            item->viewModelInstance()->advanced();
        }
    }
    ViewModelInstanceValue::advanced();
}