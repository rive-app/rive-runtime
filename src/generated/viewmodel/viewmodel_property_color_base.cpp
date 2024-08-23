#include "rive/generated/viewmodel/viewmodel_property_color_base.hpp"
#include "rive/viewmodel/viewmodel_property_color.hpp"

using namespace rive;

Core* ViewModelPropertyColorBase::clone() const
{
    auto cloned = new ViewModelPropertyColor();
    cloned->copy(*this);
    return cloned;
}
