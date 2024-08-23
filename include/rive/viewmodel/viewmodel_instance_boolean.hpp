#ifndef _RIVE_VIEW_MODEL_INSTANCE_BOOLEAN_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_BOOLEAN_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_boolean_base.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelInstanceBoolean : public ViewModelInstanceBooleanBase
{
protected:
    void propertyValueChanged() override;
};
} // namespace rive

#endif