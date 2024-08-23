#include "rive/joystick.hpp"
#include "rive/artboard.hpp"
#include "rive/transform_component.hpp"

using namespace rive;

StatusCode Joystick::onAddedDirty(CoreContext* context)
{
    StatusCode status = Super::onAddedDirty(context);
    if (status != StatusCode::Ok)
    {
        return status;
    }
    if (handleSourceId() != Core::emptyId)
    {
        auto coreObject = context->resolve(handleSourceId());
        if (coreObject == nullptr || !coreObject->is<TransformComponent>())
        {
            return StatusCode::MissingObject;
        }
        m_handleSource = static_cast<TransformComponent*>(coreObject);
    }

    return StatusCode::Ok;
}

StatusCode Joystick::onAddedClean(CoreContext* context)
{
    m_xAnimation = artboard()->animation(xId());
    m_yAnimation = artboard()->animation(yId());

    return StatusCode::Ok;
}

void Joystick::buildDependencies()
{
    // We only need to update if we're in world space (and that is only required
    // at runtime if we have a custom handle source).
    if (m_handleSource != nullptr && parent() != nullptr)
    {
        parent()->addDependent(this);
        m_handleSource->addDependent(this);
    }
}

void Joystick::update(ComponentDirt value)
{
    if (m_handleSource == nullptr)
    {
        return;
    }
    if (hasDirt(value, ComponentDirt::WorldTransform | ComponentDirt::Transform))
    {
        Mat2D world = Mat2D::fromTranslate(posX(), posY());
        if (parent() != nullptr && parent()->is<WorldTransformComponent>())
        {
            world = parent()->as<WorldTransformComponent>()->worldTransform() * world;
        }

        if (m_worldTransform != world)
        {
            m_worldTransform = world;
            m_inverseWorldTransform = world.invertOrIdentity();
        }

        auto pos = m_inverseWorldTransform * m_handleSource->worldTranslation();

        auto localBounds = AABB(-width() * originX(),
                                -height() * originY(),
                                -width() * originX() + width(),
                                -height() * originY() + height());

        auto local = localBounds.factorFrom(pos);
        x(local.x);
        y(local.y);
    }
}

void Joystick::apply(Artboard* artboard) const
{
    if (m_xAnimation != nullptr)
    {
        m_xAnimation->apply(artboard,
                            ((isJoystickFlagged(JoystickFlags::invertX) ? -x() : x()) + 1.0f) /
                                2.0f * m_xAnimation->durationSeconds());
    }
    if (m_yAnimation != nullptr)
    {

        m_yAnimation->apply(artboard,
                            ((isJoystickFlagged(JoystickFlags::invertY) ? -y() : y()) + 1.0f) /
                                2.0f * m_yAnimation->durationSeconds());
    }
}
