#include "rive/joystick.hpp"
#include "rive/artboard.hpp"
#include "rive/transform_component.hpp"
#include "rive/animation/keyed_object.hpp"

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
#ifdef WITH_RECORDER
    if(Artboard::isDebug) {
        printf("[RECORDER_LOG] Joys::added x: %u y: %u\n",
               xId(), yId());
    }
#endif
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
    if (hasDirt(value,
                ComponentDirt::WorldTransform | ComponentDirt::Transform))
    {
        Mat2D world = Mat2D::fromTranslate(posX(), posY());
        if (parent() != nullptr && parent()->is<WorldTransformComponent>())
        {
            world = parent()->as<WorldTransformComponent>()->worldTransform() *
                    world;
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
#ifdef WITH_RECORDER
    if(Artboard::isDebug) {
        printf("[RECORDER_LOG] Joys::X for m_x: %s\n",
               m_xAnimation->name().c_str());
    }
#endif
        m_xAnimation->apply(
            artboard,
            ((isJoystickFlagged(JoystickFlags::invertX) ? -x() : x()) + 1.0f) /
                2.0f * m_xAnimation->durationSeconds());
    }
    if (m_yAnimation != nullptr)
    {

#ifdef WITH_RECORDER
    if(Artboard::isDebug) {
        printf("[RECORDER_LOG] Joys::Y for m_y: %s\n",
               m_yAnimation->name().c_str());
    }
#endif
        m_yAnimation->apply(
            artboard,
            ((isJoystickFlagged(JoystickFlags::invertY) ? -y() : y()) + 1.0f) /
                2.0f * m_yAnimation->durationSeconds());
    }
    for (const auto& nestedRemapAnimation : m_dependents)
    {
        nestedRemapAnimation->advance(0, false);
    }
}

Vec2D Joystick::measureLayout(float width,
                              LayoutMeasureMode widthMode,
                              float height,
                              LayoutMeasureMode heightMode)
{
    return Vec2D(std::min((widthMode == LayoutMeasureMode::undefined
                               ? std::numeric_limits<float>::max()
                               : width),
                          Joystick::width()),
                 std::min((heightMode == LayoutMeasureMode::undefined
                               ? std::numeric_limits<float>::max()
                               : height),
                          Joystick::height()));
}

void Joystick::controlSize(Vec2D size,
                           LayoutScaleType widthScaleType,
                           LayoutScaleType heightScaleType,
                           LayoutDirection direction)
{
    width(size.x);
    height(size.y);
    posX(size.x * originX());
    posY(size.y * originY());
}

void Joystick::xChanged() { artboard()->addDirt(ComponentDirt::Components); }
void Joystick::yChanged() { artboard()->addDirt(ComponentDirt::Components); }
void Joystick::posXChanged() { artboard()->addDirt(ComponentDirt::Components); }
void Joystick::posYChanged() { artboard()->addDirt(ComponentDirt::Components); }
void Joystick::widthChanged()
{
    artboard()->addDirt(ComponentDirt::Components);
}
void Joystick::heightChanged()
{
    artboard()->addDirt(ComponentDirt::Components);
}

void Joystick::addAnimationDependents(Artboard* artboard,
                                      LinearAnimation* animation)
{

    auto totalObjects = animation->numKeyedObjects();
    for (int i = 0; i < totalObjects; i++)
    {
        auto object = animation->getObject(i);
        auto coreObject = artboard->resolve(object->objectId());
        if (coreObject != nullptr && coreObject->is<NestedRemapAnimation>())
        {
            m_dependents.push_back(coreObject->as<NestedRemapAnimation>());
        }
    }
}

void Joystick::addDependents(Artboard* artboard)
{
    if (m_yAnimation != nullptr)
    {
        addAnimationDependents(artboard, m_yAnimation);
    }
    if (m_xAnimation != nullptr)
    {
        addAnimationDependents(artboard, m_xAnimation);
    }
}