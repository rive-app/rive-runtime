#ifndef _RIVE_SHAPE_HPP_
#define _RIVE_SHAPE_HPP_

#include "rive/hit_info.hpp"
#include "rive/generated/shapes/shape_base.hpp"
#include "rive/shapes/path_composer.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/drawable_flag.hpp"
#include <vector>

namespace rive
{
class Path;
class PathComposer;
class HitTester;
class Shape : public ShapeBase, public ShapePaintContainer
{
private:
    PathComposer m_PathComposer;
    std::vector<Path*> m_Paths;
    AABB m_WorldBounds;

    bool m_WantDifferencePath = false;

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
    bool hitTest(const IAABB& area) const;

    const PathComposer* pathComposer() const { return &m_PathComposer; }
    PathComposer* pathComposer() { return &m_PathComposer; }

    void pathChanged();
    void addFlags(PathFlags flags);
    bool isFlagged(PathFlags flags) const;
    StatusCode onAddedDirty(CoreContext* context) override;
    bool isEmpty();
    void pathCollapseChanged();

    AABB worldBounds()
    {
        if ((static_cast<DrawableFlag>(drawableFlags()) & DrawableFlag::WorldBoundsClean) !=
            DrawableFlag::WorldBoundsClean)
        {
            drawableFlags(drawableFlags() |
                          static_cast<unsigned short>(DrawableFlag::WorldBoundsClean));
            m_WorldBounds = computeWorldBounds();
        }
        return m_WorldBounds;
    }
    void markBoundsDirty()
    {
        drawableFlags(drawableFlags() &
                      ~static_cast<unsigned short>(DrawableFlag::WorldBoundsClean));
    }

    AABB computeWorldBounds(const Mat2D* xform = nullptr) const;
    AABB computeLocalBounds() const;
    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
};
} // namespace rive

#endif
