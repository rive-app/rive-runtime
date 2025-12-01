#include "rive/constraints/constraint.hpp"
#include "rive/hittest_command_path.hpp"
#include "rive/shapes/deformer.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/points_path.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/shapes/paint/blend_mode.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/path_composer.hpp"
#include "rive/artboard.hpp"
#include "rive/clip_result.hpp"
#include "rive/math/contour_measure.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/profiler/profiler_macros.h"
#include <algorithm>

using namespace rive;

Shape::Shape() : m_PathComposer(this) {}

void Shape::addPath(Path* path)
{
    // Make sure the path is not already in the shape.
    assert(std::find(m_Paths.begin(), m_Paths.end(), path) == m_Paths.end());
    m_Paths.push_back(path);
}

void Shape::addFlags(PathFlags flags) { m_pathFlags |= flags; }
bool Shape::isFlagged(PathFlags flags) const
{
    return (int)(pathFlags() & flags) != 0x00;
}

bool Shape::canDeferPathUpdate()
{
    auto canDefer =
        renderOpacity() == 0 &&
        !isFlagged(PathFlags::clipping | PathFlags::neverDeferUpdate);
    if (canDefer)
    {
        // If we have a dependent Skin, don't defer the update
        for (auto d : dependents())
        {
            if (d->is<PointsPath>() && d->as<PointsPath>()->skin() != nullptr)
            {
                return false;
            }
        }
    }
    return canDefer;
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

float Shape::length()
{
    if (m_WorldLength < 0)
    {
        float l = 0;
        for (auto path : m_Paths)
        {
            const bool pathDirty = path->hasDirt(ComponentDirt::Path |
                                                 ComponentDirt::WorldTransform |
                                                 ComponentDirt::NSlicer);
            RawPath temp;
            const RawPath& base =
                pathDirty ? (path->buildPath(temp), temp) : path->rawPath();
            RawPath source = base.transform(path->pathTransform());
            ContourMeasureIter iter(&source);
            while (auto contour = iter.next())
            {
                l += contour->length();
            }
        }
        m_WorldLength = l;
    }
    return m_WorldLength;
}

void Shape::pathChanged()
{
    m_PathComposer.addDirt(ComponentDirt::Path, true);
    m_WorldLength = -1;
    for (auto constraint : constraints())
    {
        constraint->addDirt(ComponentDirt::Path);
    }
    invalidateStrokeEffects();
}

void Shape::addToRenderPath(RenderPath* path, const Mat2D& transform)
{
    if (isFlagged(PathFlags::local))
    {
        path->addPath(m_PathComposer.localPath()->renderPath(this),
                      transform * worldTransform());
    }
    else
    {
        path->addPath(m_PathComposer.worldPath()->renderPath(this), transform);
    }
}

void Shape::addToRawPath(RawPath& path, const Mat2D* transform)
{
    if (isFlagged(PathFlags::local))
    {
        Mat2D xform = transform == nullptr ? worldTransform()
                                           : (*transform) * worldTransform();
        path.addPath(*m_PathComposer.localPath()->rawPath(), &xform);
    }
    else
    {
        path.addPath(*m_PathComposer.worldPath()->rawPath(), transform);
    }
}

void Shape::draw(Renderer* renderer)
{
    RIVE_PROF_SCOPE()
    for (auto shapePaint : m_ShapePaints)
    {
        if (!shapePaint->isVisible())
        {
            continue;
        }
        auto shapePaintPath = shapePaint->pickPath(this);
        if (shapePaintPath == nullptr)
        {
            continue;
        }
        shapePaint->draw(renderer, shapePaintPath, worldTransform());
    }
}

bool Shape::hitTestAABB(const Vec2D& position)
{
    return worldBounds().contains(position);
}

bool Shape::hitTestHiFi(const Vec2D& position, float hitRadius)
{
    auto hitArea = AABB(position.x - hitRadius,
                        position.y - hitRadius,
                        position.x + hitRadius,
                        position.y + hitRadius)
                       .round();
    HitTestCommandPath tester(hitArea);

    for (auto path : m_Paths)
    {
        if (!path->isCollapsed())
        {
            tester.setXform(path->pathTransform());
            path->rawPath().addTo(&tester);
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

    const bool shapeIsLocal =
        isFlagged(PathFlags::local | PathFlags::localClockwise);

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

        auto paintIsLocal =
            shapePaint->isFlagged(PathFlags::local | PathFlags::localClockwise);

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
            path->rawPath().addTo(&tester);
        }
        if (tester.wasHit())
        {
            return this;
        }
    }
    return nullptr;
}

bool Shape::hitTestPoint(const Vec2D& position,
                         bool skipOnUnclipped,
                         bool isPrimaryHit)
{
    // If we're NOT the primary hit test, don't perform the AABB hit test
    // just keep walking up the tree
    if (!isPrimaryHit)
    {
        return Component::hitTestPoint(position, skipOnUnclipped, isPrimaryHit);
    }
    // Only perform the AABB hit test if we're the primary hit test
    // This prevents walking up the tree and having another shape return a
    // false hit test because we're not hitting their AABB
    if (hitTestAABB(position) &&
        Component::hitTestPoint(position, skipOnUnclipped, isPrimaryHit))
    {
        return hitTestHiFi(position, 2);
    }
    return false;
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

StatusCode Shape::onAddedClean(CoreContext* context)
{
    StatusCode code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    // Find the deformer, if any.
    m_deformer = nullptr;
    for (auto currentParent = parent(); currentParent != nullptr;
         currentParent = currentParent->parent())
    {
        RenderPathDeformer* deformer = RenderPathDeformer::from(currentParent);
        if (deformer)
        {
            m_deformer = deformer;
            return StatusCode::Ok;
        }
    }

    return StatusCode::Ok;
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

bool Shape::willDraw() { return Super::willDraw() && renderOpacity() != 0.0f; }

// Do constraints need to be marked as dirty too? From tests it doesn't seem
// they do.
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
    void addPath(CommandPath* path, const Mat2D& transform) override
    {
        assert(false);
    }

    void moveTo(float x, float y) override { m_rawPath.moveTo(x, y); }
    void lineTo(float x, float y) override { m_rawPath.lineTo(x, y); }
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
        override
    {
        m_rawPath.cubicTo(ox, oy, ix, iy, x, y);
    }
    void close() override { m_rawPath.close(); }

    RenderPath* renderPath() override
    {
        assert(false);
        return nullptr;
    }

    const RenderPath* renderPath() const override
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
        path->rawPath().addTo(&boundsCalculator);

        AABB aabb = boundsCalculator.bounds(
            xform == nullptr ? path->pathTransform()
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

Vec2D Shape::measureLayout(float width,
                           LayoutMeasureMode widthMode,
                           float height,
                           LayoutMeasureMode heightMode)
{
    Vec2D size = Vec2D();
    for (auto path : m_Paths)
    {
        Vec2D measured =
            path->measureLayout(width, widthMode, height, heightMode);
        size =
            Vec2D(std::max(size.x, measured.x), std::max(size.y, measured.y));
    }
    return size;
}

ShapePaintPath* Shape::worldPath() { return m_PathComposer.worldPath(); }
ShapePaintPath* Shape::localPath() { return m_PathComposer.localPath(); }
ShapePaintPath* Shape::localClockwisePath()
{
    return m_PathComposer.localClockwisePath();
}

Component* Shape::pathBuilder() { return &m_PathComposer; }