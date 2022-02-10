#include "rive/generated/nested_artboard_base.hpp"
#include "rive/nested_artboard.hpp"

using namespace rive;

Core* NestedArtboardBase::clone() const
{
    auto cloned = new NestedArtboard();
    cloned->copy(*this);
    return cloned;
}
