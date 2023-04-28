#ifndef _RIVE_PATH_HPP_
#define _RIVE_PATH_HPP_
#include "rive/command_path.hpp"
#include "rive/generated/shapes/path_base.hpp"
#include "rive/math/mat2d.hpp"
#include <vector>

namespace rive
{
class Shape;
class PathVertex;

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
    std::unique_ptr<CommandPath> m_CommandPath;
    std::vector<PathVertex*> m_Vertices;
    bool m_deferredPathDirt = false;

public:
    Shape* shape() const { return m_Shape; }
    StatusCode onAddedClean(CoreContext* context) override;
    void buildDependencies() override;
    virtual const Mat2D& pathTransform() const;
    CommandPath* commandPath() const { return m_CommandPath.get(); }
    void update(ComponentDirt value) override;

    void addVertex(PathVertex* vertex);

    virtual void markPathDirty();
    virtual bool isPathClosed() const { return true; }
    void onDirty(ComponentDirt dirt) override;
#ifdef ENABLE_QUERY_FLAT_VERTICES
    FlattenedPath* makeFlat(bool transformToParent);
#endif

#ifdef TESTING
    std::vector<PathVertex*>& vertices() { return m_Vertices; }
#endif

    // pour ourselves into a command-path
    void buildPath(CommandPath&) const;
};
} // namespace rive

#endif
