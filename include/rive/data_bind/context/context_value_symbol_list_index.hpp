#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_SYMBOL_LIST_INDEX_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_SYMBOL_LIST_INDEX_HPP_
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_values/data_value_symbol_list_index.hpp"
namespace rive
{
class DataBindContextValueSymbolListIndex : public DataBindContextValue
{

public:
    DataBindContextValueSymbolListIndex(DataBind* m_dataBind);
    void apply(Core* component,
               uint32_t propertyKey,
               bool isMainDirection) override;
    bool syncTargetValue(Core* target, uint32_t propertyKey) override;

private:
    uint32_t m_previousValue = 0;
    DataValueSymbolListIndex m_targetDataValue;

protected:
    DataValue* targetValue() override { return &m_targetDataValue; }
};
} // namespace rive

#endif