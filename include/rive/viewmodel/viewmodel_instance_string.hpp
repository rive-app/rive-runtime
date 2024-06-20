#ifndef _RIVE_VIEW_MODEL_INSTANCE_STRING_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_STRING_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_string_base.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelInstanceString : public ViewModelInstanceStringBase
{
public:
    void propertyValueChanged() override;
};
} // namespace rive

#endif