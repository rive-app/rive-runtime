#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_BOOLEAN_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_BOOLEAN_HPP_
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
namespace rive
{
class DataBindContextValueBoolean : public DataBindContextValue
{
public:
    DataBindContextValueBoolean(DataBind* m_dataBind);
    void apply(Core* component,
               uint32_t propertyKey,
               bool isMainDirection) override;
    bool syncTargetValue(Core* target, uint32_t propertyKey) override;

private:
    bool m_previousValue = false;
    DataValueBoolean m_targetDataValue;

protected:
    DataValue* targetValue() override { return &m_targetDataValue; }
};
} // namespace rive

#endif