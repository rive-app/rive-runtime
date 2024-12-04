#include "rive/generated/constraints/scrolling/clamped_scroll_physics_base.hpp"
#include "rive/constraints/scrolling/clamped_scroll_physics.hpp"

using namespace rive;

Core* ClampedScrollPhysicsBase::clone() const
{
    auto cloned = new ClampedScrollPhysics();
    cloned->copy(*this);
    return cloned;
}
