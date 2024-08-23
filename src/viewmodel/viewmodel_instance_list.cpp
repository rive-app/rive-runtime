#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/component_dirt.hpp"

using namespace rive;

void ViewModelInstanceList::propertyValueChanged() { addDirt(ComponentDirt::Components); }

void ViewModelInstanceList::addItem(ViewModelInstanceListItem* item)
{
    m_ListItems.push_back(item);
    propertyValueChanged();
}

void ViewModelInstanceList::insertItem(int index, ViewModelInstanceListItem* item)
{
    // TODO: @hernan decide if we want to return a boolean
    if (index < m_ListItems.size())
    {
        m_ListItems.insert(m_ListItems.begin() + index, item);
        propertyValueChanged();
    }
}

void ViewModelInstanceList::removeItem(int index)
{
    // TODO: @hernan decide if we want to return a boolean
    if (index < m_ListItems.size())
    {
        m_ListItems.erase(m_ListItems.begin() + index);
        propertyValueChanged();
    }
}

void ViewModelInstanceList::removeItem(ViewModelInstanceListItem* listItem)
{
    auto noSpaceEnd = std::remove(m_ListItems.begin(), m_ListItems.end(), listItem);
    m_ListItems.erase(noSpaceEnd, m_ListItems.end());
    propertyValueChanged();
}

ViewModelInstanceListItem* ViewModelInstanceList::item(uint32_t index)
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
        std::iter_swap(m_ListItems.begin() + index1, m_ListItems.begin() + index2);
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
        cloned->addItem(clonedValue);
    }
    return cloned;
}