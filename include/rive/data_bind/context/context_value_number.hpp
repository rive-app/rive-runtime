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
};
} // namespace rive

#endif