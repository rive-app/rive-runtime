#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_BOOLEAN_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_BOOLEAN_HPP_
#include "rive/data_bind/context/context_value.hpp"
namespace rive
{
class DataBindContextValueBoolean : public DataBindContextValue
{

public:
    DataBindContextValueBoolean(ViewModelInstanceValue* value);
    void apply(Core* component, uint32_t propertyKey) override;
    virtual void applyToSource(Core* component, uint32_t propertyKey) override;

private:
    bool m_Value;
};
} // namespace rive

#endif