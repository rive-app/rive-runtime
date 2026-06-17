#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_ANY_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_ANY_HPP_
#include "rive/data_bind/context/context_value.hpp"
namespace rive
{
class DataBindContextValueAny : public DataBindContextValue
{

public:
    DataBindContextValueAny(DataBind* dataBind);
    void apply(Core* component,
               uint32_t propertyKey,
               bool isMainDirection,
               DataBind* dataBind) override;
};
} // namespace rive

#endif