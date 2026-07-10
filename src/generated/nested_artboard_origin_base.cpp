#include "rive/generated/nested_artboard_origin_base.hpp"
#include "rive/nested_artboard_origin.hpp"

using namespace rive;

Core* NestedArtboardOriginBase::clone() const
{
    auto cloned = new NestedArtboardOrigin();
    cloned->copy(*this);
    return cloned;
}
