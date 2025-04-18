#include "rive/data_bind/context/context_value_list.hpp"
#include "rive/data_bind/context/context_value_list_item.hpp"
#include "rive/data_bind/data_bind_list_item_provider.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueList::DataBindContextValueList(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

DataBindContextValueList::~DataBindContextValueList()
{
    for (auto& item : m_ListItemsCache)
    {
        item.reset();
    }
}

void DataBindContextValueList::insertItem(Core* target,
                                          ViewModelInstanceListItem* listItem,
                                          int index)
{
    std::unique_ptr<DataBindContextValueListItem> cacheListItem =
        rivestd::make_unique<DataBindContextValueListItem>(listItem);
    if (index == -1)
    {
        m_ListItemsCache.push_back(std::move(cacheListItem));
    }
    else
    {
        m_ListItemsCache.insert(m_ListItemsCache.begin() + index,
                                std::move(cacheListItem));
    }
}

void DataBindContextValueList::swapItems(Core* target, int index1, int index2)
{
    std::iter_swap(m_ListItemsCache.begin() + index1,
                   m_ListItemsCache.begin() + index2);
}

void DataBindContextValueList::popItem(Core* target)
{
    m_ListItemsCache.pop_back();
}

void DataBindContextValueList::update(Core* target)
{
    if (target != nullptr)
    {
        auto source = m_dataBind->source();
        auto sourceList = source->as<ViewModelInstanceList>();
        auto listItems = sourceList->listItems();

        int listIndex = 0;
        while (listIndex < listItems.size())
        {
            auto listItem = listItems[listIndex];
            if (listIndex < m_ListItemsCache.size())
            {
                if (m_ListItemsCache[listIndex]->listItem() == listItem)
                {
                    // Same item in same position: do nothing
                }
                else
                {
                    int cacheIndex = listIndex + 1;
                    bool found = false;
                    while (cacheIndex < m_ListItemsCache.size())
                    {
                        if (m_ListItemsCache[cacheIndex]->listItem() ==
                            listItem)
                        {
                            // swap cache position with new item
                            swapItems(target, listIndex, cacheIndex);
                            found = true;
                            break;
                        }
                        cacheIndex++;
                    }
                    if (!found)
                    {
                        // create new element and insert it in listIndex
                        insertItem(target, listItem, listIndex);
                    }
                }
            }
            else
            {
                // create new element and cache the listItem in listIndex
                insertItem(target, listItem, -1);
            }

            listIndex++;
        }
        // remove remaining cached elements backwars to pop from the vector.
        listIndex = m_ListItemsCache.size() == 0
                        ? 0
                        : (int)(m_ListItemsCache.size() - 1);
        while ((size_t)listIndex > listItems.size())
        {
            popItem(target);
            listIndex--;
        }

        auto provider = DataBindListItemProvider::from(target);
        if (provider != nullptr)
        {
            std::vector<ViewModelInstanceListItem*> items;
            for (auto& item : m_ListItemsCache)
            {
                items.push_back(item.get()->listItem());
            }
            provider->updateList(m_dataBind->propertyKey(), items);
        }
    }
}

void DataBindContextValueList::apply(Core* target,
                                     uint32_t propertyKey,
                                     bool isMainDirection)
{}

void DataBindContextValueList::applyToSource(Core* target,
                                             uint32_t propertyKey,
                                             bool isMainDirection)
{
    // TODO: @hernan does applyToSource make sense? Should we block it somehow?
}