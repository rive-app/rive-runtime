#include "rive/constraints/constraint.hpp"
#include "rive/hittest_command_path.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/shapes/paint/blend_mode.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/path_composer.hpp"
#include <algorithm>

using namespace rive;

Shape::Shape() : m_PathComposer(this) {}

void Shape::addPath(Path* path)
{
    // Make sure the path is not already in the shape.
    assert(std::find(m_Paths.begin(), m_Paths.end(), path) == m_Paths.end());
    m_Paths.push_back(path);
}

bool Shape::canDeferPathUpdate()
{
    return renderOpacity() == 0 && (pathSpace() & PathSpace::Clipping) != PathSpace::Clipping &&
           (pathSpace() & PathSpace::FollowPath) != PathSpace::FollowPath;
}

void Shape::update(ComponentDirt value)
{
    Super::update(value);

    if (hasDirt(value, ComponentDirt::RenderOpacity))
    {
        propagateOpacity(renderOpacity());
    }
}

bool Shape::collapse(bool value)
{
    if (!Super::collapse(value))
    {
        return false;
    }
    m_PathComposer.collapse(value);
    return true;
}

void Shape::pathChanged()
{
    m_PathComposer.addDirt(ComponentDirt::Path, true);
    for (auto constraint : constraints())
    {
        constraint->addDirt(ComponentDirt::Path);
    }
    invalidateStrokeEffects();
}

void Shape::addToRenderPath(RenderPath* path, const Mat2D& transform)
{
    auto space = pathSpace();
    if ((space & PathSpace::Local) == PathSpace::Local)
    {
        path->addPath(m_PathComposer.localPath(), transform * worldTransform());
    }
    else
    {
        path->addPath(m_PathComposer.worldPath(), transform);
    }
}

void Shape::draw(Renderer* renderer)
{
    if (renderOpacity() == 0.0f)
    {
        return;
    }
    auto shouldRestore = clip(renderer);

    for (auto shapePaint : m_ShapePaints)
    {
        if (!shapePaint->isVisible())
        {
            continue;
        }
        renderer->save();
        bool paintsInLocal = (shapePaint->pathSpace() & PathSpace::Local) == PathSpace::Local;
        if (paintsInLocal)
        {
            renderer->transform(worldTransform());
        }
        shapePaint->draw(renderer,
                         paintsInLocal ? m_PathComposer.localPath() : m_PathComposer.worldPath());
        renderer->restore();
    }

    if (shouldRestore)
    {
        renderer->restore();
    }
}

bool Shape::hitTest(const IAABB& area) const
{
    HitTestCommandPath tester(area);

    for (auto path : m_Paths)
    {
        tester.setXform(path->pathTransform());
        path->buildPath(tester);
    }
    return tester.wasHit();
}

Core* Shape::hitTest(HitInfo* hinfo, const Mat2D& xform)
{
    if (renderOpacity() == 0.0f)
    {
        return nullptr;
    }

    // TODO: clip:

    const bool shapeIsLocal = (pathSpace() & PathSpace::Local) == PathSpace::Local;

    for (auto rit = m_ShapePaints.rbegin(); rit != m_ShapePaints.rend(); ++rit)
    {
        auto shapePaint = *rit;
        if (shapePaint->isTranslucent())
        {
            continue;
        }
        if (!shapePaint->isVisible())
        {
            continue;
        }

        auto paintIsLocal = (shapePaint->pathSpace() & PathSpace::Local) == PathSpace::Local;

        auto mx = xform;
        if (paintIsLocal)
        {
            mx *= worldTransform();
        }

        HitTestCommandPath tester(hinfo->area);

        for (auto path : m_Paths)
        {
            if (shapeIsLocal)
            {
                tester.setXform(xform * path->pathTransform());
            }
            else
            {
                tester.setXform(mx * path->pathTransform());
            }
            path->buildPath(tester);
        }
        if (tester.wasHit())
        {
            return this;
        }
    }
    return nullptr;
}

void Shape::buildDependencies()
{
    // Make sure to propagate the call to PathComposer as it's no longer part of
    // Core and owned only by the Shape.
    m_PathComposer.buildDependencies();

    Super::buildDependencies();

    // Set the blend mode on all the shape paints. If we ever animate this
    // property, we'll need to update it in the update cycle/mark dirty when the
    // blend mode changes.
    for (auto paint : m_ShapePaints)
    {
        paint->blendMode(blendMode());
    }
}

void Shape::addDefaultPathSpace(PathSpace space) { m_DefaultPathSpace |= space; }

StatusCode Shape::onAddedDirty(CoreContext* context)
{
    auto code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    // This ensures context propagates to path composer too.
    return m_PathComposer.onAddedDirty(context);
}
