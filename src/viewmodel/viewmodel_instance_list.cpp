#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/component_dirt.hpp"

using namespace rive;

ViewModelInstanceList::~ViewModelInstanceList() {}

void ViewModelInstanceList::propertyValueChanged()
{
    addDirt(ComponentDirt::Bindings);
    onValueChanged();
}

void ViewModelInstanceList::addItem(rcp<ViewModelInstanceListItem> item)
{
    m_ListItems.push_back(item);
    propertyValueChanged();
}

bool ViewModelInstanceList::addItemAt(rcp<ViewModelInstanceListItem> item,
                                      int index)
{
    if (index <= m_ListItems.size())
    {
        m_ListItems.insert(m_ListItems.begin() + index, item);
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
}

void ViewModelInstanceList::removeItem(int index)
{
    // TODO: @hernan decide if we want to return a boolean
    if (index >= 0 && index < m_ListItems.size())
    {
        auto listItem = m_ListItems[index];
        m_ListItems.erase(m_ListItems.begin() + index);
        propertyValueChanged();
    }
}

void ViewModelInstanceList::removeItem(rcp<ViewModelInstanceListItem> listItem)
{
    auto noSpaceEnd =
        std::remove(m_ListItems.begin(), m_ListItems.end(), listItem);
    m_ListItems.erase(noSpaceEnd, m_ListItems.end());
    propertyValueChanged();
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