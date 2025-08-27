#include "rive/generated/foreground_layout_drawable_base.hpp"
#include "rive/foreground_layout_drawable.hpp"

using namespace rive;

Core* ForegroundLayoutDrawableBase::clone() const
{
    auto cloned = new ForegroundLayoutDrawable();
    cloned->copy(*this);
    return cloned;
}
