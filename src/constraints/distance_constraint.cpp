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

    const Vec2D targetTranslation = m_Target->worldTranslation();
    const Vec2D ourTranslation = component->worldTranslation();

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

    Mat2D& world = component->mutableWorldTransform();
    Vec2D position = targetTranslation + toTarget;
    position = Vec2D::lerp(ourTranslation, position, strength());
    world[4] = position.x;
    world[5] = position.y;
}
