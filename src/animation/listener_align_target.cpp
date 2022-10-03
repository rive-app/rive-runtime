#include "rive/animation/listener_align_target.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/node.hpp"
#include "rive/constraints/constraint.hpp"

using namespace rive;

void ListenerAlignTarget::perform(StateMachineInstance* stateMachineInstance, Vec2D position) const
{
    auto coreTarget = stateMachineInstance->artboard()->resolve(targetId());
    if (coreTarget == nullptr || !coreTarget->is<Node>())
    {
        return;
    }
    auto target = coreTarget->as<Node>();
    Mat2D targetParentWorld = getParentWorld(*target);
    Mat2D inverse;
    if (!targetParentWorld.invert(&inverse))
    {
        return;
    }

    auto localPosition = inverse * position;
    target->x(localPosition.x);
    target->y(localPosition.y);
}
