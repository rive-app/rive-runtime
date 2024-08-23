#ifndef _RIVE_VIEW_MODEL_INSTANCE_COLOR_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_COLOR_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_color_base.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelInstanceColor : public ViewModelInstanceColorBase
{
public:
    void propertyValueChanged() override;
};
} // namespace rive

#endif