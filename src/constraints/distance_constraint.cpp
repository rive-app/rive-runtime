#include "rive/constraints/distance_constraint.hpp"
#include "rive/bones/bone.hpp"
#include "rive/artboard.hpp"
#include <algorithm>

using namespace rive;

enum class Mode
{
    Closer = 0,
    Further = 1,
    Exact = 2
};

void DistanceConstraint::constrain(TransformComponent* component)
{
    if (m_Target == nullptr || m_Target->isCollapsed())
    {
        return;
    }

    Mat2D& world = component->mutableWorldTransform();
    const Vec2D anchor = component->localAnchor();
    const Vec2D anchorWorld = Vec2D(world[0] * anchor.x + world[2] * anchor.y,
                                    world[1] * anchor.x + world[3] * anchor.y);

    const Vec2D targetTranslation = m_Target->worldTranslation();
    const Vec2D ourTranslation = component->worldTranslation() + anchorWorld;

    Vec2D toTarget = ourTranslation - targetTranslation;
    float currentDistance = toTarget.length();

    switch (static_cast<Mode>(modeValue()))
    {
        case Mode::Closer:
            if (currentDistance < distance())
            {
                return;
            }
            break;
        case Mode::Further:
            if (currentDistance > distance())
            {
                return;
            }
            break;
        case Mode::Exact:
            break;
    }
    if (currentDistance < 0.001f)
    {
        return;
    }

    toTarget *= (distance() / currentDistance);

    Vec2D position = targetTranslation + toTarget;
    position = Vec2D::lerp(ourTranslation, position, strength());
    world[4] = position.x - anchorWorld.x;
    world[5] = position.y - anchorWorld.y;
}

void DistanceConstraint::distanceChanged() { markConstraintDirty(); }

void DistanceConstraint::modeValueChanged() { markConstraintDirty(); }