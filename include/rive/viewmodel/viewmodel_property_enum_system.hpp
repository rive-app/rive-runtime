#ifndef _RIVE_VIEW_MODEL_PROPERTY_ENUM_SYSTEM_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_ENUM_SYSTEM_HPP_
#include "rive/generated/viewmodel/viewmodel_property_enum_system_base.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelPropertyEnumSystem : public ViewModelPropertyEnumSystemBase
{
public:
    DataEnum* dataEnum() override { return &m_systemDataEnum; }

private:
    static DataEnum m_systemDataEnum;
};
} // namespace rive

#endif