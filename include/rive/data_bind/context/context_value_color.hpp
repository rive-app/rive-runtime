#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_COLOR_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_COLOR_HPP_
#include "rive/data_bind/context/context_value.hpp"
namespace rive
{
class DataBindContextValueColor : public DataBindContextValue
{

public:
    DataBindContextValueColor(ViewModelInstanceValue* source, DataConverter* converter);
    void apply(Core* component, uint32_t propertyKey, bool isMainDirection) override;
    DataValue* getTargetValue(Core* target, uint32_t propertyKey) override;
};
} // namespace rive

#endif