#ifndef _RIVE_VIEW_MODEL_PROPERTY_ENUM_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_ENUM_HPP_
#include "rive/generated/viewmodel/viewmodel_property_enum_base.hpp"
#include "rive/viewmodel/data_enum.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelPropertyEnum : public ViewModelPropertyEnumBase
{

public:
    std::string value(std::string name);
    std::string value(uint32_t index);
    bool value(std::string name, std::string value);
    bool value(uint32_t index, std::string value);
    int valueIndex(std::string name);
    int valueIndex(uint32_t index);
    void dataEnum(DataEnum* value);
    DataEnum* dataEnum();

private:
    DataEnum* m_DataEnum;
};

} // namespace rive

#endif