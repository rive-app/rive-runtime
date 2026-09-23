#ifndef _RIVE_SHAPE_HPP_
#define _RIVE_SHAPE_HPP_

#include "rive/hit_info.hpp"
#include "rive/generated/shapes/shape_base.hpp"
#include "rive/shapes/path_composer.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/layout/layout_participant.hpp"
#include "rive/drawable_flag.hpp"
#include <vector>

namespace rive
{
class Path;
class PathComposer;
class HitTester;
class RenderPathDeformer;

class Shape : public ShapeBase, public ShapePaintContainer
{
private:
    PathComposer m_PathComposer;
    std::vector<Path*> m_Paths;
    AABB m_WorldBounds;
    // Memoized like m_WorldBounds, but computed on demand from a const getter,
    // so the cache itself is mutable. Invalidated by markBoundsDirty(), by
    // pathChanged() -- which Path raises for both of its inputs, geometry
    // (markPathDirty) and path transform (onDirty) -- and by
    // pathCollapseChanged(), which changes which paths are measured at all. Our
    // own world transform moving dirties our paths' world transforms, so that
    // routes here too.
    mutable AABB m_LocalBounds;
    mutable bool m_LocalBoundsClean = false;
    float m_WorldLength = -1;

    bool m_WantDifferencePath = false;
    bool m_hasLayoutParticipant = false;
    RenderPathDeformer* m_deformer = nullptr;

    // Scale-to-fit and the memoized intrinsic bounds live on the
    // LayoutParticipant, not here: only a participant uses them, and it is
    // already allocated for exactly that case — so a plain Shape carries none
    // of it. Invalidated via pathChanged(), which Path raises for both geometry
    // (markPathDirty) and path transform (onDirty) changes — the two inputs.
    void updateLayoutScale(Vec2D size);
    void invalidateIntrinsicBounds();

    Artboard* getArtboard() override { return artboard(); }

public:
    Shape();
    void buildDependencies() override;
    void addChild(Component* component) override;
    void additiveAmountChanged() override;
    void syncShapePaintBlendModes();
    bool collapse(bool value) override;
    bool canDeferPathUpdate();
    void addPath(Path* path);
    void addToRenderPath(RenderPath* commandPath, const Mat2D& transform);
    void addToRawPath(RawPath& rawPath, const Mat2D* transform);
    std::vector<Path*>& paths() { return m_Paths; }

    bool wantDifferencePath() const { return m_WantDifferencePath; }

    void update(ComponentDirt value) override;
    void draw(Renderer* renderer) override;
    bool willDraw() override;
    Core* hitTest(HitInfo*, const Mat2D&) override;

    const PathComposer* pathComposer() const { return &m_PathComposer; }
    PathComposer* pathComposer() { return &m_PathComposer; }

    RenderPathDeformer* deformer() const { return m_deformer; }

    void pathChanged();
    void addFlags(PathFlags flags);
    bool isFlagged(PathFlags flags) const;
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;
    bool isEmpty();
    void pathCollapseChanged();

    float length() override;
    void setLength(float value) override {}

    AABB localBounds() const override
    {
        if (!m_LocalBoundsClean)
        {
            m_LocalBoundsClean = true;
            m_LocalBounds = computeLocalBounds();
        }
        return m_LocalBounds;
    }
    AABB worldBounds()
    {
        if ((static_cast<DrawableFlag>(drawableFlags()) &
             DrawableFlag::WorldBoundsClean) != DrawableFlag::WorldBoundsClean)
        {
            drawableFlags(
                drawableFlags() |
                static_cast<unsigned short>(DrawableFlag::WorldBoundsClean));
            m_WorldBounds = computeWorldBounds();
        }
        return m_WorldBounds;
    }
    void markBoundsDirty()
    {
        drawableFlags(drawableFlags() & ~static_cast<unsigned short>(
                                            DrawableFlag::WorldBoundsClean));
        m_LocalBoundsClean = false;
        m_WorldLength = -1;
#ifdef WITH_RIVE_LAYOUT
        // A participant's intrinsic bounds drive its layout slot, so
        // re-measure/re-solve when they change.
        if (auto* participant = layoutParticipant())
        {
            participant->markLayoutNodeDirty();
        }
#endif
    }

    // Combined path bounds in world space. xform, when given, is applied
    // after each path's world transform -- pass an inverse world transform to
    // measure in this shape's space (what computeLocalBounds does).
    AABB computeWorldBounds(const Mat2D* xform = nullptr) const;
    AABB computeLocalBounds() const;
    // Combined path bounds in this shape's local space, computed from each
    // path's local transform (fold-independent, valid even at scale 0).
    AABB computeIntrinsicBounds() const;
    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
    void controlSize(Vec2D size,
                     LayoutScaleType widthScaleType,
                     LayoutScaleType heightScaleType,
                     LayoutDirection direction) override;

    // Participation via an optional LayoutParticipant child.
    LayoutParticipant* layoutParticipant() const;
    bool isParticipatingInLayout() const;
    void composeWorldTransform() override;

protected:
    void updateConstraints() override;

public:
    Vec2D layoutBaseTranslation(LayoutParticipant* participant) const;

    bool hitTestAABB(const Vec2D& position);
    bool hitTestHiFi(const Vec2D& position, float hitRadius);
    bool hitTestPoint(const Vec2D& position,
                      bool skipOnUnclipped,
                      bool isPrimaryHit) override;
    // Implemented for ShapePaintContainer.
    const Mat2D& shapeWorldTransform() const override
    {
        return worldTransform();
    }

    ShapePaintPath* worldPath() override;
    ShapePaintPath* localPath() override;
    ShapePaintPath* localClockwisePath() override;
    Component* pathBuilder() override;
};
} // namespace rive

#endif
