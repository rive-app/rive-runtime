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

void DataBindContextValueList::apply(Core* target,
                                     uint32_t propertyKey,
                                     bool isMainDirection)
{
    syncSourceValue();
    auto value =
        calculateValue<DataValueList, std::vector<ViewModelInstanceListItem*>*>(
            m_dataValue,
            isMainDirection,
            m_dataBind);
    if (target != nullptr)
    {
        auto provider = DataBindListItemProvider::from(target);
        if (provider != nullptr)
        {
            provider->updateList(m_dataBind->propertyKey(), value);
        }
    }
}

void DataBindContextValueList::applyToSource(Core* target,
                                             uint32_t propertyKey,
                                             bool isMainDirection)
{
    // TODO: @hernan does applyToSource make sense? Should we block it somehow?
}