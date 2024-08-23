#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_BOOLEAN_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_BOOLEAN_HPP_
#include "rive/data_bind/context/context_value.hpp"
namespace rive
{
class DataBindContextValueBoolean : public DataBindContextValue
{

public:
    DataBindContextValueBoolean(ViewModelInstanceValue* source, DataConverter* converter);
    void apply(Core* component, uint32_t propertyKey, bool isMainDirection) override;
    DataValue* getTargetValue(Core* target, uint32_t propertyKey) override;
};
} // namespace rive

#endif