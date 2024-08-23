#include "rive/generated/layout/n_slicer_base.hpp"
#include "rive/layout/n_slicer.hpp"

using namespace rive;

Core* NSlicerBase::clone() const
{
    auto cloned = new NSlicer();
    cloned->copy(*this);
    return cloned;
}
