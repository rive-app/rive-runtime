#include "rive/data_bind/context/context_value_symbol_list_index.hpp"
#include "rive/data_bind/data_values/data_value_symbol_list_index.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindContextValueSymbolListIndex::DataBindContextValueSymbolListIndex(
    DataBind* dataBind) :
    DataBindContextValue(dataBind)
{}

void DataBindContextValueSymbolListIndex::apply(Core* target,
                                                uint32_t propertyKey,
                                                bool isMainDirection)
{
    syncSourceValue();
    auto value =
        calculateValue<DataValueSymbolListIndex, uint32_t>(m_dataValue,
                                                           isMainDirection,
                                                           m_dataBind);
    switch (CoreRegistry::propertyFieldId(propertyKey))
    {
        case CoreDoubleType::id:
            CoreRegistry::setDouble(target, propertyKey, (float)value);
            break;
        case CoreUintType::id:
            CoreRegistry::setUint(target, propertyKey, value);
            break;
    }
}