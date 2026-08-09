#include "rive/constraints/constraint.hpp"
#include "rive/hittest_command_path.hpp"
#include "rive/shapes/deformer.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/points_path.hpp"
#include "rive/shapes/parametric_path.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/layout/layout_participant.hpp"
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
    invalidateIntrinsicBounds();
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
    // Collapsed paths are skipped when measuring, so the bounds change.
    invalidateIntrinsicBounds();
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
    invalidateIntrinsicBounds();
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
    RIVE_PROF_SCOPE_L(2)
    auto needsSaveOperation = m_needsSaveOperation || m_ShapePaints.size() > 1;
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
        shapePaint->draw(renderer,
                         shapePaintPath,
                         worldTransform(),
                         false,
                         nullptr,
                         needsSaveOperation);
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

    // Tight curve bounds (solves cubic extrema) rather than the control-point
    // box, so a participant sizing to its geometry scales to fill exactly.
    AABB preciseBounds(const Mat2D& xform)
    {
        m_rawPath.transformInPlace(xform);
        return m_rawPath.preciseBounds();
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

AABB Shape::computeIntrinsicBounds() const
{
    // Only a participant caches (and only a participant calls this); without
    // one we just compute, so a plain Shape stores nothing.
    auto* participant = layoutParticipant();
    if (participant != nullptr && participant->hostBoundsValid())
    {
        return participant->hostBounds();
    }
    // Like computeWorldBounds but in this shape's local space, using each
    // path's local transform directly instead of round-tripping through our
    // (non-invertible when the layout fold is 0) world transform.
    bool first = true;
    AABB computedBounds = AABB::forExpansion();

    ComputeBoundsCommandPath boundsCalculator;
    RawPath pendingPath;
    bool usedPendingBuild = false;
    for (auto path : m_Paths)
    {
        if (path->isCollapsed())
        {
            continue;
        }
        AABB aabb;
        AABB propertyBounds;
        if (!path->needsPathBuild())
        {
            path->rawPath().addTo(&boundsCalculator);
            aabb = boundsCalculator.preciseBounds(path->transform());
            boundsCalculator.rewind();
        }
        else if (path->tryPropertyBounds(propertyBounds))
        {
            // Layout (and a participant's fit scale) runs before Path::update
            // in the update pass. A parametric path only positions its vertices
            // there, so building it here would measure an unpositioned path —
            // take its declared box instead, which is what it will occupy.
            usedPendingBuild = true;
            aabb = path->transform().mapBoundingBox(propertyBounds);
        }
        else
        {
            // Vertex-driven: the vertices are authored, so building a throwaway
            // copy measures real geometry. Without this a fill participant
            // computes a scale of 1 and renders at its hugged size on frame 1.
            usedPendingBuild = true;
            pendingPath.rewind();
            path->buildPath(pendingPath);
            pendingPath.addTo(&boundsCalculator);
            aabb = boundsCalculator.preciseBounds(path->transform());
            boundsCalculator.rewind();
        }
        // An empty (vertex-less) path leaves preciseBounds at its expansion
        // sentinel, which is inverted (+/-FLT_MAX). Folding that in would
        // poison the bounds, and a participant's anchor derives from them —
        // pushing its world translation to -FLT_MAX. Written as !(>= 0) so a
        // NaN is rejected too; a real but degenerate path (zero width or
        // height) still counts.
        if (!(aabb.width() >= 0.0f && aabb.height() >= 0.0f))
        {
            continue;
        }
        if (first)
        {
            first = false;
            computedBounds = aabb;
        }
        else
        {
            computedBounds.expand(aabb);
        }
    }

    AABB bounds = first ? AABB() : computedBounds;
    if (participant != nullptr)
    {
        // Anything measured before its build is provisional: a declared box can
        // be wider than the precise geometry (a polygon is inscribed in it).
        // Return it, but don't cache it, so the precise bounds win once the
        // paths build — nothing invalidates on the build itself, only when dirt
        // is added.
        participant->hostBounds(bounds, /*cache*/ !usedPendingBuild);
    }
    return bounds;
}

void Shape::invalidateIntrinsicBounds()
{
    if (auto* participant = layoutParticipant())
    {
        participant->invalidateHostBounds();
    }
}

static ParametricPath* firstParametricPath(std::vector<Path*>& paths)
{
    for (auto path : paths)
    {
        if (path->is<ParametricPath>())
        {
            return path->as<ParametricPath>();
        }
    }
    return nullptr;
}

Vec2D Shape::measureLayout(float width,
                           LayoutMeasureMode widthMode,
                           float height,
                           LayoutMeasureMode heightMode)
{
#ifdef WITH_RIVE_LAYOUT
    // A participant sizes to its combined bounds (all paths); controlSize then
    // scales those bounds to fill the slot.
    if (isParticipatingInLayout())
    {
        AABB bounds = computeIntrinsicBounds();
        return Vec2D(bounds.width(), bounds.height());
    }
#endif
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

void Shape::controlSize(Vec2D size,
                        LayoutScaleType widthScaleType,
                        LayoutScaleType heightScaleType,
                        LayoutDirection direction)
{
#ifdef WITH_RIVE_LAYOUT
    // A participant scales its combined bounds to fill the slot.
    if (isParticipatingInLayout())
    {
        updateLayoutScale(size);
        return;
    }
#endif
    // Content: a parametric shape's size lives on its ParametricPath child.
    if (auto* path = firstParametricPath(m_Paths))
    {
        path->controlSize(size, widthScaleType, heightScaleType, direction);
    }
}

void Shape::updateLayoutScale(Vec2D size)
{
    // Only reached from the participant branch of controlSize.
    auto* participant = layoutParticipant();
    if (participant == nullptr)
    {
        return;
    }
    AABB bounds = computeIntrinsicBounds();
    float w = bounds.width();
    float h = bounds.height();
    // intrinsicBounds is from local geometry (not the world round-trip) so it
    // stays valid even at scale 0.
    float newScaleX = w > 0.0f ? size.x / w : 1.0f;
    float newScaleY = h > 0.0f ? size.y / h : 1.0f;
    if (newScaleX != participant->hostScaleX() ||
        newScaleY != participant->hostScaleY())
    {
        participant->hostScale(newScaleX, newScaleY);
        markWorldTransformDirty();
    }
}

// The whole shape scales to fit, so place the scaled combined-bounds top-left
// at the slot (the origin is irrelevant once we scale).
LayoutParticipant* Shape::layoutParticipant() const
{
    for (auto* child : children())
    {
        if (child->is<LayoutParticipant>())
        {
            return child->as<LayoutParticipant>();
        }
    }
    return nullptr;
}

bool Shape::isParticipatingInLayout() const
{
    return layoutParticipant() != nullptr;
}

Vec2D Shape::layoutBaseTranslation(LayoutParticipant* participant) const
{
    assert(participant != nullptr);
    AABB intrinsic = computeIntrinsicBounds();
    return Vec2D(participant->resolvedLeft() -
                     intrinsic.left() * participant->hostScaleX(),
                 participant->resolvedTop() -
                     intrinsic.top() * participant->hostScaleY());
}

void Shape::composeWorldTransform()
{
#ifdef WITH_RIVE_LAYOUT
    auto* participant = layoutParticipant();
    if (participant != nullptr && m_ParentTransformComponent != nullptr)
    {
        // Scale innermost so it fits the geometry to the resolved box, with
        // our own transform composing on top.
        float scaleX = participant->hostScaleX();
        float scaleY = participant->hostScaleY();
        Mat2D base = Mat2D::fromTranslation(layoutBaseTranslation(participant));
        m_WorldTransform = m_ParentTransformComponent->worldTransform() * base *
                           m_Transform * Mat2D::fromScale(scaleX, scaleY);
        return;
    }
#endif
    Super::composeWorldTransform();
}

void Shape::updateConstraints()
{
#ifdef WITH_RIVE_LAYOUT
    auto* participant = layoutParticipant();
    if (participant != nullptr)
    {
        participant->applyLayoutConstraints();
    }
#endif
    Super::updateConstraints();
}

ShapePaintPath* Shape::worldPath() { return m_PathComposer.worldPath(); }
ShapePaintPath* Shape::localPath() { return m_PathComposer.localPath(); }
ShapePaintPath* Shape::localClockwisePath()
{
    return m_PathComposer.localClockwisePath();
}

Component* Shape::pathBuilder() { return &m_PathComposer; }