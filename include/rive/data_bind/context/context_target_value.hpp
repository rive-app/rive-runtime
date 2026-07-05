#ifndef _RIVE_DATA_BIND_CONTEXT_TARGET_VALUE_HPP_
#define _RIVE_DATA_BIND_CONTEXT_TARGET_VALUE_HPP_
#include <stdio.h>
#include "rive/data_bind/data_values/data_value.hpp"
namespace rive
{
class DataBind;
// `final` documents (and enforces) that no subclasses exist — letting us drop
// the virtual destructor and save the 8 B vtable pointer per instance.
// One DBCTV is embedded in every DataBindContextValue, so this scales with
// DataBind count.
class DataBindContextTargetValue final
{
private:
    DataValue* m_targetValue = nullptr;
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
    ~DataBindContextTargetValue();
    void initialize(DataBind* dataBind);
    bool syncTargetValue(DataBind* dataBind);
    DataValue* dataValue() { return m_targetValue; }
};
} // namespace rive

#endif