#include "rive/animation/listener_align_target.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/node.hpp"
#include "rive/constraints/constraint.hpp"

using namespace rive;

void ListenerAlignTarget::perform(StateMachineInstance* stateMachineInstance,
                                  const ListenerInvocation& invocation) const
{
    Vec2D position;
    Vec2D previousPosition;
    if (const PointerInvocation* p = invocation.asPointer())
    {
        position = p->position;
        previousPosition = p->previousPosition;
    }
    else
    {
        position = Vec2D(0, 0);
        previousPosition = Vec2D(0, 0);
    }
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
    if (preserveOffset())
    {

        auto localPosition = inverse * position;
        auto prevLocalPosition = inverse * previousPosition;
        target->x(target->x() + localPosition.x - prevLocalPosition.x);
        target->y(target->y() + localPosition.y - prevLocalPosition.y);
    }
    else
    {
        auto localPosition = inverse * position;
        target->x(localPosition.x);
        target->y(localPosition.y);
    }
}
