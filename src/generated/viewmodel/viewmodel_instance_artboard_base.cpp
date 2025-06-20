#include "rive/generated/viewmodel/viewmodel_instance_artboard_base.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"

using namespace rive;

Core* ViewModelInstanceArtboardBase::clone() const
{
    auto cloned = new ViewModelInstanceArtboard();
    cloned->copy(*this);
    return cloned;
}
