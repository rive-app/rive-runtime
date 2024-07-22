#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_HPP_
#include "rive/refcnt.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include <stdio.h>
namespace rive
{
class DataBindContextValue
{
protected:
    ViewModelInstanceValue* m_Source;

public:
    DataBindContextValue(){};
    virtual ~DataBindContextValue(){};
    virtual void applyToSource(Core* component, uint32_t propertyKey){};
    virtual void apply(Core* component, uint32_t propertyKey){};
    virtual void update(Core* component){};
};
} // namespace rive

#endif