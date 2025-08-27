#include "rive/data_bind/context/context_value_list.hpp"
#include "rive/data_bind/data_bind_list_item_consumer.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"

using namespace rive;

DataBindContextValueList::DataBindContextValueList(DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

void DataBindContextValueList::apply(Core* target,
                                     uint32_t propertyKey,
                                     bool isMainDirection)
{
    syncSourceValue();
    auto value = calculateValue<DataValueList,
                                std::vector<rcp<ViewModelInstanceListItem>>*>(
        m_dataValue,
        isMainDirection,
        m_dataBind);
    if (target != nullptr)
    {
        auto consumer = DataBindListItemConsumer::from(target);
        if (consumer != nullptr)
        {
            consumer->updateList(value);
        }
    }
}

void DataBindContextValueList::applyToSource(Core* target,
                                             uint32_t propertyKey,
                                             bool isMainDirection)
{
    // TODO: @hernan does applyToSource make sense? Should we block it somehow?
}