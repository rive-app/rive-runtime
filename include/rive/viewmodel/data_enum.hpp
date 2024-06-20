#ifndef _RIVE_DATA_ENUM_HPP_
#define _RIVE_DATA_ENUM_HPP_
#include "rive/generated/viewmodel/data_enum_base.hpp"
#include "rive/viewmodel/data_enum_value.hpp"
#include <stdio.h>
namespace rive
{
class DataEnum : public DataEnumBase
{
private:
    std::vector<DataEnumValue*> m_Values;

public:
    void addValue(DataEnumValue* value);
    std::vector<DataEnumValue*> values() { return m_Values; };
    std::string value(std::string name);
    std::string value(uint32_t index);
    bool value(std::string name, std::string value);
    bool value(uint32_t index, std::string value);
    int valueIndex(std::string name);
    int valueIndex(uint32_t index);
};
} // namespace rive

#endif