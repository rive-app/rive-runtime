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
};
} // namespace rive

#endif