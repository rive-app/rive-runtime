#ifndef _RIVE_VIEW_MODEL_INSTANCE_ENUM_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ENUM_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_enum_base.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelInstanceEnum : public ViewModelInstanceEnumBase
{
public:
    bool value(std::string name);
    bool value(uint32_t index);

protected:
    void propertyValueChanged() override;
};
} // namespace rive

#endif