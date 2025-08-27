#include "rive/generated/viewmodel/viewmodel_property_artboard_base.hpp"
#include "rive/viewmodel/viewmodel_property_artboard.hpp"

using namespace rive;

Core* ViewModelPropertyArtboardBase::clone() const
{
    auto cloned = new ViewModelPropertyArtboard();
    cloned->copy(*this);
    return cloned;
}
