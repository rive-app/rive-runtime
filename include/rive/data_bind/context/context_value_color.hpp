#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_COLOR_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_COLOR_HPP_
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_values/data_value_color.hpp"
namespace rive
{
class DataBindContextValueColor : public DataBindContextValue
{

public:
    DataBindContextValueColor(DataBind* m_dataBind);
    void apply(Core* component,
               uint32_t propertyKey,
               bool isMainDirection) override;
};
} // namespace rive

#endif