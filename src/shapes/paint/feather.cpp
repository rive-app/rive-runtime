#include "rive/shapes/paint/feather.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/artboard.hpp"

#include "rive/core_context.hpp"

using namespace rive;

bool Feather::validate(CoreContext* context)
{
    if (!Super::validate(context))
    {
        return false;
    }
    auto coreObject = context->resolve(parentId());
    // we know it's not nullptr from Super::validate
    assert(coreObject != nullptr);
    return coreObject->is<ShapePaint>();
}

StatusCode Feather::onAddedDirty(CoreContext* context)
{
    auto code = Super::onAddedDirty(context);

    parent()->as<ShapePaint>()->feather(this);
    return code;
}

void Feather::update(ComponentDirt value)
{
    auto shapePaint = parent()->as<ShapePaint>();
    if (hasDirt(value, ComponentDirt::Paint))
    {
        auto renderPaint = shapePaint->renderPaint();
        renderPaint->feather(strength());
    }

    if (hasDirt(value, ComponentDirt::WorldTransform | ComponentDirt::Path))
    {
        bool offsetInArtboard = space() == TransformSpace::world;
        if (inner())
        {
            auto shape = ShapePaintContainer::from(shapePaint->parent());

            if (shape != nullptr)
            {
                auto transform = shape->shapeWorldTransform();
                auto path = shapePaint->pickPath(shape);
                if (path == nullptr)
                {
                    return;
                }
                rebuildInnerPath(path, transform, offsetInArtboard);
                // Mark dirty so draw() re-applies the effect path override if
                // one is active (update() only has the original shape path).
                m_effectPathDirty = true;
            }
#ifdef TESTING
            renderCount++;
#endif
            return;
        }
    }
}

void Feather::rebuildInnerPath(const ShapePaintPath* path,
                               const Mat2D& shapeTransform,
                               bool offsetInArtboard)
{
    m_effectPathDirty = false;
    auto bounds = path->rawPath()->bounds().pad(strength() * 1.5f);
    Vec2D innerOffset(offsetX(), offsetY());
    if (offsetInArtboard)
    {
        Mat2D inverseTransform = shapeTransform.invertOrIdentity();
        innerOffset = Vec2D::transformDir(innerOffset, inverseTransform);
    }
    m_innerPath.rewind();
    m_innerPath.addRect(bounds);
    Mat2D innerOffsetTransform = Mat2D::fromTranslation(innerOffset);
    m_innerPath.addPathBackwards(*path->rawPath(), &innerOffsetTransform);
}

void Feather::buildDependencies()
{
    auto shape = parent()->as<ShapePaint>()->parent();
    if (shape == nullptr)
    {
        return;
    }
    if (shape->is<Shape>())
    {
        shape->as<Shape>()->pathComposer()->addDependent(this);
    }
    else
    {
        shape->addDependent(this);
    }
}

void Feather::strengthChanged()
{
    addDirt(inner() ? (ComponentDirt)(ComponentDirt::Paint |
                                      ComponentDirt::WorldTransform)
                    : ComponentDirt::Paint);
}
void Feather::offsetXChanged() { strengthChanged(); }
void Feather::offsetYChanged() { strengthChanged(); }