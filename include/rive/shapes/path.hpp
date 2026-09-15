#ifndef _RIVE_PATH_HPP_
#define _RIVE_PATH_HPP_
#include "rive/command_path.hpp"
#include "rive/generated/shapes/path_base.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include <vector>

namespace rive
{
class Shape;
class PathVertex;
class RenderPathDeformer;

#ifdef ENABLE_QUERY_FLAT_VERTICES
/// Optionally compiled in for tools that need to compute per frame world
/// transformed path vertices. These should not be used at runtime as it's
/// not optimized for performance (it does a lot of memory allocation).

/// A flattened path is composed of only linear
/// and cubic vertices. No corner vertices and it's entirely in world space.
/// This is helpful for getting a close to identical representation of the
/// vertices used to issue the high level path draw commands.
class FlattenedPath
{
private:
    std::vector<PathVertex*> m_Vertices;

public:
    ~FlattenedPath();

    const std::vector<PathVertex*>& vertices() const { return m_Vertices; }
    void addVertex(PathVertex* vertex, const Mat2D& transform);
};
#endif

class Path : public PathBase
{
protected:
    Shape* m_Shape = nullptr;
    std::vector<PathVertex*> m_Vertices;
    bool m_deferredPathDirt = false;
    PathFlags m_pathFlags = PathFlags::none;
    RawPath m_rawPath;
    RenderPathDeformer* deformer() const;
    void isHoleChanged() override;

public:
    static float computeIdealControlPointDistance(const Vec2D& toPrev,
                                                  const Vec2D& toNext,
                                                  float radius);

    static void addRoundedRect(RawPath& rawPath,
                               const AABB& bounds,
                               float topLeft,
                               float topRight,
                               float bottomRight,
                               float bottomLeft);

    Shape* shape() const { return m_Shape; }
    StatusCode onAddedClean(CoreContext* context) override;
    void buildDependencies() override;
    virtual const Mat2D& pathTransform() const;
    bool collapse(bool value) override;
    const RawPath& rawPath() const { return m_rawPath; }
    // True while m_rawPath has yet to be rebuilt for pending changes: the Path
    // dirt is still queued, or the build was deferred. Layout runs before
    // Path::update in the update pass, so a measure can land here first and
    // must build its own copy rather than read a stale/empty rawPath.
    bool needsPathBuild() const
    {
        return hasDirt(ComponentDirt::Path) || m_deferredPathDirt;
    }
    // Bounds in this path's own space derived from its properties rather than
    // its built geometry, for callers that run before update() has positioned
    // vertices. A ParametricPath knows its box up front; anything vertex-driven
    // returns false and must be measured from geometry.
    virtual bool tryPropertyBounds(AABB& result) const { return false; }
    void update(ComponentDirt value) override;

    void addFlags(PathFlags flags);
    bool isFlagged(PathFlags flags) const;

    bool canDeferPathUpdate();
    void addVertex(PathVertex* vertex);
    void popVertex();

    virtual void markPathDirty(bool sendToLayout = true);
    virtual bool isPathClosed() const { return true; }
    void onDirty(ComponentDirt dirt) override;
    inline bool isHidden() const { return (pathFlags() & 0x1) == 0x1; }
#ifdef ENABLE_QUERY_FLAT_VERTICES
    FlattenedPath* makeFlat(bool transformToParent);
#endif

#ifdef TESTING
    std::vector<PathVertex*>& vertices() { return m_Vertices; }
#endif

#ifdef WITH_RIVE_EDITOR
    /// Editor-only: sort `m_Vertices` by sibling FractionalIndex
    /// (`Component::childOrder().compareTo(...)`). The runtime `.riv` import
    /// path adds vertices in file declaration order — which the
    /// exporter writes in the authored order, so the resulting
    /// path geometry is correct without any sorting. Coop
    /// delivers vertices in server-batch arrival order, which is
    /// arbitrary; without this resort the rendered path zigzags
    /// across the children, producing twisted / self-intersecting
    /// shapes (the visible "broken pose" we see in the editor's
    /// pump-driven render). Mirrors Dart's
    /// `ContainerComponent.sortChildren` /
    /// `Path._sortVertices` flow that runs as part of cleanDirt
    /// (rive_file.dart:607-613).
    void sortVerticesForEditor();
    /// Idempotent add — no-op if `vertex` is already on
    /// `m_Vertices`. Called by `PathVertex::editorParentChanged`
    /// when its parent transitions to this Path (initial hydration
    /// or re-parent target). The dedupe matters for cross-batch
    /// retries and mixed-mode (`.riv`-imported + coop-edited) files.
    void addVertexForEditor(PathVertex* vertex);
    /// Remove `vertex` from `m_Vertices` if present. Called by
    /// `PathVertex::editorParentChanged` when its parent transitions
    /// AWAY from this Path (re-parent source, parent-of-parent
    /// deletion, or child deletion).
    void removeVertexForEditor(PathVertex* vertex);
#endif

    void buildPath(RawPath&) const;
    AABB localBounds() const override;
};
} // namespace rive

#endif
