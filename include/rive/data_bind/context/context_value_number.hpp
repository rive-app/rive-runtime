#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_NUMBER_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_NUMBER_HPP_
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
namespace rive
{
class DataBindContextValueNumber : public DataBindContextValue
{

public:
    DataBindContextValueNumber(DataBind* m_dataBind);
    void apply(Core* component,
               uint32_t propertyKey,
               bool isMainDirection) override;
    bool syncTargetValue(Core* target, uint32_t propertyKey) override;

private:
    float m_previousValue = 0;
    DataValueNumber m_targetDataValue;

protected:
    DataValue* targetValue() override { return &m_targetDataValue; }
};
} // namespace rive

#endif