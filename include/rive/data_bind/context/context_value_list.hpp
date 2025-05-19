#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_LIST_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_LIST_HPP_
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_values/data_value_list.hpp"

namespace rive
{
class DataBindContextValueList : public DataBindContextValue
{

public:
    DataBindContextValueList(DataBind* m_dataBind);
    void apply(Core* component,
               uint32_t propertyKey,
               bool isMainDirection) override;
    virtual void applyToSource(Core* component,
                               uint32_t propertyKey,
                               bool isMainDirection) override;
};
} // namespace rive

#endif