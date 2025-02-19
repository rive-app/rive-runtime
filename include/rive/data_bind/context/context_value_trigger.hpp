#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_TRIGGER_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_TRIGGER_HPP_
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_values/data_value_trigger.hpp"
namespace rive
{
class DataBindContextValueTrigger : public DataBindContextValue
{

public:
    DataBindContextValueTrigger(DataBind* m_dataBind);
    void apply(Core* component,
               uint32_t propertyKey,
               bool isMainDirection) override;
    bool syncTargetValue(Core* target, uint32_t propertyKey) override;

private:
    uint32_t m_previousValue = 0;
    DataValueTrigger m_targetDataValue;

protected:
    DataValue* targetValue() override { return &m_targetDataValue; }
};
} // namespace rive

#endif