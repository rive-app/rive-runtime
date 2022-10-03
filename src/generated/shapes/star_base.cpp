#include "rive/generated/shapes/star_base.hpp"
#include "rive/shapes/star.hpp"

using namespace rive;

Core* StarBase::clone() const
{
    auto cloned = new Star();
    cloned->copy(*this);
    return cloned;
}
