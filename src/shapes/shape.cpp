#include "rive/constraints/constraint.hpp"
#include "rive/hittest_command_path.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/shapes/paint/blend_mode.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/path_composer.hpp"
#include "rive/clip_result.hpp"
#include "rive/math/raw_path.hpp"
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
    ClipResult clipResult = clip(renderer);

    if (clipResult != ClipResult::emptyClip)
    {
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
                             paintsInLocal ? m_PathComposer.localPath()
                                           : m_PathComposer.worldPath());
            renderer->restore();
        }
    }

    if (clipResult != ClipResult::noClip)
    {
        renderer->restore();
    }
}

bool Shape::hitTest(const IAABB& area) const
{
    HitTestCommandPath tester(area);

    for (auto path : m_Paths)
    {
        if (!path->isCollapsed())
        {
            tester.setXform(path->pathTransform());
            path->buildPath(tester);
        }
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

bool Shape::isEmpty()
{
    for (auto path : m_Paths)
    {
        if (!path->isHidden() && !path->isCollapsed())
        {
            return false;
        }
    }
    return true;
}

// Do constraints need to be marked as dirty too? From tests it doesn't seem they do.
void Shape::pathCollapseChanged() { m_PathComposer.pathCollapseChanged(); }

class ComputeBoundsCommandPath : public CommandPath
{
public:
    ComputeBoundsCommandPath() {}

    AABB bounds(const Mat2D& xform)
    {
        m_rawPath.transformInPlace(xform);
        return m_rawPath.bounds();
    }

    void rewind() override { m_rawPath.rewind(); }
    void fillRule(FillRule value) override {}
    void addPath(CommandPath* path, const Mat2D& transform) override { assert(false); }

    void moveTo(float x, float y) override { m_rawPath.moveTo(x, y); }
    void lineTo(float x, float y) override { m_rawPath.lineTo(x, y); }
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override
    {
        m_rawPath.cubicTo(ox, oy, ix, iy, x, y);
    }
    void close() override { m_rawPath.close(); }

    RenderPath* renderPath() override
    {
        assert(false);
        return nullptr;
    }

private:
    RawPath m_rawPath;
};

AABB Shape::computeWorldBounds(const Mat2D* xform) const
{
    bool first = true;
    AABB computedBounds = AABB::forExpansion();

    ComputeBoundsCommandPath boundsCalculator;
    for (auto path : m_Paths)
    {
        if (path->isCollapsed())
        {
            continue;
        }

        path->buildPath(boundsCalculator);

        AABB aabb = boundsCalculator.bounds(xform == nullptr ? path->pathTransform()
                                                             : path->pathTransform() * *xform);

        if (first)
        {
            first = false;
            computedBounds = aabb;
        }
        else
        {
            computedBounds.expand(aabb);
        }
        boundsCalculator.rewind();
    }

    return computedBounds;
}

AABB Shape::computeLocalBounds() const
{
    const Mat2D& world = worldTransform();
    Mat2D inverseWorld = world.invertOrIdentity();
    return computeWorldBounds(&inverseWorld);
}