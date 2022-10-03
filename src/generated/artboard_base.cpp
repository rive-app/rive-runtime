#include "rive/generated/artboard_base.hpp"
#include "rive/artboard.hpp"

using namespace rive;

Core* ArtboardBase::clone() const
{
    auto cloned = new Artboard();
    cloned->copy(*this);
    return cloned;
}
