#include "rive/generated/shapes/paint/fill_base.hpp"
#include "rive/shapes/paint/fill.hpp"

using namespace rive;

Core* FillBase::clone() const
{
    auto cloned = new Fill();
    cloned->copy(*this);
    return cloned;
}
