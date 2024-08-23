#ifndef _RIVE_VIEW_MODEL_INSTANCE_NUMBER_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_NUMBER_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_number_base.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelInstanceNumber : public ViewModelInstanceNumberBase
{
protected:
    void propertyValueChanged() override;
};
} // namespace rive

#endif