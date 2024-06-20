#ifndef _RIVE_DATA_ENUM_VALUE_HPP_
#define _RIVE_DATA_ENUM_VALUE_HPP_
#include "rive/generated/viewmodel/data_enum_value_base.hpp"
#include <stdio.h>
namespace rive
{
class DataEnumValue : public DataEnumValueBase
{
public:
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif