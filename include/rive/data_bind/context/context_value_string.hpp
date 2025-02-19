#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_STRING_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_STRING_HPP_
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
namespace rive
{
class DataBindContextValueString : public DataBindContextValue
{

public:
    DataBindContextValueString(DataBind* m_dataBind);
    void apply(Core* component,
               uint32_t propertyKey,
               bool isMainDirection) override;
    bool syncTargetValue(Core* target, uint32_t propertyKey) override;

private:
    std::string m_previousValue = "";
    DataValueString m_targetDataValue;

protected:
    DataValue* targetValue() override { return &m_targetDataValue; }
};
} // namespace rive

#endif