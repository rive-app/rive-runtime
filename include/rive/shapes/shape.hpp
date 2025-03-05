#ifndef _RIVE_SHAPE_HPP_
#define _RIVE_SHAPE_HPP_

#include "rive/hit_info.hpp"
#include "rive/generated/shapes/shape_base.hpp"
#include "rive/animation/hittable.hpp"
#include "rive/shapes/path_composer.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/drawable_flag.hpp"
#include <vector>

namespace rive
{
class Path;
class PathComposer;
class HitTester;
class RenderPathDeformer;

class Shape : public ShapeBase, public ShapePaintContainer, public Hittable
{
private:
    PathComposer m_PathComposer;
    std::vector<Path*> m_Paths;
    AABB m_WorldBounds;
    float m_WorldLength = -1;

    bool m_WantDifferencePath = false;
    RenderPathDeformer* m_deformer = nullptr;

    Artboard* getArtboard() override { return artboard(); }

public:
    Shape();
    void buildDependencies() override;
    bool collapse(bool value) override;
    bool canDeferPathUpdate();
    void addPath(Path* path);
    void addToRenderPath(RenderPath* commandPath, const Mat2D& transform);
    std::vector<Path*>& paths() { return m_Paths; }

    bool wantDifferencePath() const { return m_WantDifferencePath; }

    void update(ComponentDirt value) override;
    void draw(Renderer* renderer) override;
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
        m_WorldLength = -1;
    }

    AABB computeWorldBounds(const Mat2D* xform = nullptr) const;
    AABB computeLocalBounds() const;
    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;

    bool hitTestAABB(const Vec2D& position) override;
    bool hitTestHiFi(const Vec2D& position, float hitRadius) override;
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
