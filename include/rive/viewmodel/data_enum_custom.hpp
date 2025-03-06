#ifndef _RIVE_DATA_ENUM_CUSTOM_HPP_
#define _RIVE_DATA_ENUM_CUSTOM_HPP_
#include "rive/generated/viewmodel/data_enum_custom_base.hpp"
#include <stdio.h>
namespace rive
{
class DataEnumCustom : public DataEnumCustomBase
{
public:
    const std::string& enumName() const override { return name(); }
};
} // namespace rive

#endif