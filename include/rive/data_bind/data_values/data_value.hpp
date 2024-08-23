#ifndef _RIVE_DATA_VALUE_HPP_
#define _RIVE_DATA_VALUE_HPP_
#include "rive/data_bind/data_values/data_type.hpp"

#include <stdio.h>
namespace rive
{
class DataValue
{
public:
    virtual bool isTypeOf(DataType dataType) const { return false; }
    template <typename T> inline bool is() const { return isTypeOf(T::typeKey); }
    template <typename T> inline T* as()
    {
        assert(is<T>());
        return static_cast<T*>(this);
    }
};
} // namespace rive

#endif