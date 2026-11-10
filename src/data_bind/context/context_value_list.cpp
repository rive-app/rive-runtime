#include "rive/data_bind/context/context_value_list.hpp"
#include "rive/data_bind/data_bind_list_item_consumer.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"

using namespace rive;

DataBindContextValueList::DataBindContextValueList(DataBind* dataBind)
    : DataBindContextValue(dataBind)
{}

void DataBindContextValueList::apply(
    Core* target,
    uint32_t propertyKey,
    bool isMainDirection,
    DataBind* dataBind)
{
  syncSourceValue(dataBind);
  auto value = calculateValue<
      DataValueList,
      std::vector<rcp<ViewModelInstanceListItem>>*>(
      m_dataValue,
      isMainDirection,
      dataBind);
  if (target != nullptr)
  {
    auto consumer = DataBindListItemConsumer::from(target);
    if (consumer != nullptr)
    {
      // A converter's list can depend on the row it's made in (an operand
      // bound to the row's own values), so its source alone doesn't say
      // what it holds.
      consumer->boundListSource(
          dataBind->converter() == nullptr ? dataBind->source() : nullptr);
      consumer->updateList(value);
    }
  }
}
