#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_COLOR_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_COLOR_HPP_
#include "rive/data_bind/context/context_value.hpp"
namespace rive
{
class DataBindContextValueColor : public DataBindContextValue
{

public:
    DataBindContextValueColor(ViewModelInstanceValue* value);
    void apply(Component* component, uint32_t propertyKey) override;
    virtual void applyToSource(Component* component, uint32_t propertyKey) override;

private:
    double m_Value;
};
} // namespace rive

#endif