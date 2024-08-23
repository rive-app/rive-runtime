#ifndef _RIVE_VIEW_MODEL_PROPERTY_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_HPP_
#include "rive/generated/viewmodel/viewmodel_property_base.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelProperty : public ViewModelPropertyBase
{
public:
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif