#include "rive/generated/scripted/scripted_layout_base.hpp"
#include "rive/scripted/scripted_layout.hpp"

using namespace rive;

Core* ScriptedLayoutBase::clone() const
{
    auto cloned = new ScriptedLayout();
    cloned->copy(*this);
    return cloned;
}
