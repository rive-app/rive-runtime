#include "rive/generated/shapes/image_base.hpp"
#include "rive/shapes/image.hpp"

using namespace rive;

Core* ImageBase::clone() const
{
    auto cloned = new Image();
    cloned->copy(*this);
    return cloned;
}
