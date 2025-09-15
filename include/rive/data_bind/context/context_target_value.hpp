#ifndef _RIVE_DATA_BIND_CONTEXT_TARGET_VALUE_HPP_
#define _RIVE_DATA_BIND_CONTEXT_TARGET_VALUE_HPP_
#include <stdio.h>
#include "rive/data_bind/data_values/data_value.hpp"
namespace rive
{
class DataBind;
class DataBindContextTargetValue
{
private:
    DataValue* m_targetValue = nullptr;
    DataBind* m_dataBind = nullptr;
    template <typename T = DataValue, typename U> bool updateValue(U value)
    {
        if (m_targetValue->as<T>()->value() != value)
        {
            m_targetValue->as<T>()->value(value);
            return true;
        }
        return false;
    }

public:
    virtual ~DataBindContextTargetValue();
    void initialize(DataBind* dataBind);
    bool syncTargetValue();
    DataValue* dataValue() { return m_targetValue; }
};
} // namespace rive

#endif