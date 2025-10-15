#include "rive/generated/scripted/scripted_drawable_base.hpp"
#include "rive/scripted/scripted_drawable.hpp"

using namespace rive;

Core* ScriptedDrawableBase::clone() const
{
    auto cloned = new ScriptedDrawable();
    cloned->copy(*this);
    return cloned;
}
