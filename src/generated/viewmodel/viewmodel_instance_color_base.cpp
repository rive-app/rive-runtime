#include "rive/generated/viewmodel/viewmodel_instance_color_base.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"

using namespace rive;

Core* ViewModelInstanceColorBase::clone() const
{
    auto cloned = new ViewModelInstanceColor();
    cloned->copy(*this);
    return cloned;
}
