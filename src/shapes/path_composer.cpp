#include "rive/shapes/path_composer.hpp"
#include "rive/artboard.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/path.hpp"
#include <algorithm>
#include <limits>
#include "rive/shapes/shape.hpp"
#include "rive/factory.hpp"
#include "rive/shapes/points_path.hpp"

using namespace rive;

PathComposer::PathComposer(Shape* shape) :
    m_shape(shape),
    m_localPath(true),
    m_worldPath(false),
    m_localClockwisePath(true),
    m_deferredPathDirt(false)
{}

void PathComposer::buildDependencies()
{
    assert(m_shape != nullptr);
    m_shape->addDependent(this);
    for (auto path : m_shape->paths())
    {
        path->addDependent(this);
    }
}

void PathComposer::onDirty(ComponentDirt dirt)
{
    if (m_deferredPathDirt && !m_shapeNotified)
    {
        // We'd deferred the update, let's make sure the rest of our
        // dependencies update too. Constraints need to update too, stroke
        // effects, etc.
        m_shapeNotified = true;
        m_shape->pathChanged();
    }
}

// A local path is each path's geometry mapped by inverseShapeWorld *
// pathTransform -- the path's transform relative to the shape. A rigid move of
// the shape (or of any ancestor) leaves every one of those unchanged, so the
// local path it would rebuild is identical to the one it already holds.
// Rebuilding it anyway costs the copy and, worse, bumps the RenderPath's
// mutation id, which throws away its cached triangulation. Snapshot the inputs
// and compare.
bool PathComposer::localInputsChanged()
{
    auto& paths = m_shape->paths();
    const Mat2D& world = m_shape->worldTransform();
    const Mat2D inverseWorld = world.invertOrIdentity();
    // The composed transform is inverseWorld * pathTransform, and the error
    // in it is dominated by the inverse: its linear part scales the path's
    // coordinates, and inverting a shrunk-down world amplifies everything by
    // 1/det. So bound the tolerance by the terms that actually produce the
    // value rather than by the world transform alone.
    // 16 ULPs of the terms above. A sweep over 1..1024 ULPs on twelve real
    // files moved the skip rate by less than 3 points, so this is nowhere near
    // a knife edge.
    constexpr float kULPs = 16.0f * std::numeric_limits<float>::epsilon();
    const float inverseLinear = std::max({std::abs(inverseWorld[0]),
                                          std::abs(inverseWorld[1]),
                                          std::abs(inverseWorld[2]),
                                          std::abs(inverseWorld[3])});
    const float inverseTranslation =
        std::max(std::abs(inverseWorld[4]), std::abs(inverseWorld[5]));

    m_scratchInputs.clear();
    m_scratchInputs.reserve(paths.size());
    for (auto path : paths)
    {
        const Mat2D& pathWorld = path->pathTransform();
        const float pathLinear = std::max({std::abs(pathWorld[0]),
                                           std::abs(pathWorld[1]),
                                           std::abs(pathWorld[2]),
                                           std::abs(pathWorld[3])});
        const float pathTranslation =
            std::max(std::abs(pathWorld[4]), std::abs(pathWorld[5]));
        const float linearTolerance =
            kULPs * std::max(1.0f, inverseLinear * pathLinear);
        const float translationTolerance =
            kULPs *
            std::max(1.0f,
                     inverseLinear * pathTranslation + inverseTranslation);
        m_scratchInputs.push_back({inverseWorld * pathWorld,
                                   path->geometryVersion(),
                                   path->isHidden() || path->isCollapsed(),
                                   linearTolerance,
                                   translationTolerance});
    }

    // A shape can gain a local path flag after we have already snapshotted
    // (a fill added, a clip registered), so a block that has never been built
    // against this snapshot has to rebuild no matter what the inputs say.
    const PathFlags localFlags =
        m_shape->pathFlags() & (PathFlags::local | PathFlags::localClockwise);
    bool changed = !m_hasLocalInputs || m_localInputs.size() != paths.size() ||
                   (localFlags & ~m_builtLocalFlags) != PathFlags::none;
    for (size_t i = 0; !changed && i < paths.size(); i++)
    {
        changed = !m_localInputs[i].matches(m_scratchInputs[i]);
    }
    if (changed)
    {
        // Only adopt the new inputs when we are about to rebuild from them.
        // Holding the ones the buffer was actually built from is what keeps the
        // tolerance from accumulating frame over frame.
        m_localInputs = m_scratchInputs;
        m_hasLocalInputs = true;
        m_builtLocalFlags = localFlags;
    }
    return changed;
}

void PathComposer::update(ComponentDirt value)
{
    m_shapeNotified = false;
    if (hasDirt(value, ComponentDirt::Path | ComponentDirt::NSlicer))
    {
        if (m_shape->canDeferPathUpdate())
        {
            m_deferredPathDirt = true;
            return;
        }
        m_deferredPathDirt = false;

        // Only the local blocks consult the snapshot, so a shape with just a
        // world path (a clip source, a world stroke) should not pay for the
        // inverse and the per-path compare. A shape that gains a local flag
        // later still lands here with no snapshot, which reads as changed.
        const bool rebuildLocal =
            m_shape->isFlagged(PathFlags::local | PathFlags::localClockwise) &&
            localInputsChanged();
        if (m_shape->isFlagged(PathFlags::local) && rebuildLocal)
        {
            m_localPath.rewind();
            auto world = m_shape->worldTransform();
            Mat2D inverseWorld = world.invertOrIdentity();
            // Get all the paths into local shape space.
            for (auto path : m_shape->paths())
            {
                if (!path->isHidden() && !path->isCollapsed())
                {
                    const auto localTransform =
                        inverseWorld * path->pathTransform();
                    m_localPath.addPath(path->rawPath(), &localTransform);
                }
            }
        }
        if (m_shape->isFlagged(PathFlags::localClockwise) && rebuildLocal)
        {
            m_localClockwisePath.rewind();
            auto world = m_shape->worldTransform();
            Mat2D inverseWorld = world.invertOrIdentity();
            // Get all the paths into local shape space.
            for (auto path : m_shape->paths())
            {
                if (path->isHidden() || path->isCollapsed())
                {
                    continue;
                }
                const auto localTransform =
                    inverseWorld * path->pathTransform();
                bool isNotClockwise =
                    path->is<PointsPath>() &&
                    localTransform.determinant() *
                            path->as<PointsPath>()->winding() <
                        0;
                bool isHole = path->isHole();
                // Only draw backwards if values are different
                if (isNotClockwise != isHole)
                {
                    m_localClockwisePath.addPathBackwards(path->rawPath(),
                                                          &localTransform);
                }
                else
                {
                    m_localClockwisePath.addPath(path->rawPath(),
                                                 &localTransform);
                }
            }
        }
        if (m_shape->isFlagged(PathFlags::world))
        {
            m_worldPath.rewind();

            for (auto path : m_shape->paths())
            {
                if (!path->isHidden() && !path->isCollapsed())
                {
                    const Mat2D& transform = path->pathTransform();
                    m_worldPath.addPath(path->rawPath(), &transform);
                }
            }
        }
        m_shape->markBoundsDirty();
    }
}

// Instead of adding dirt and rely on the recursive behavior of the addDirt
// method, we need to explicitly add dirt to the dependents. The reason is that
// a collapsed shape will not clear its dirty path flag in the current frame
// since it is collapsed. So in a future frame if it is uncollapsed, we mark its
// path flag as dirty again, but since it was already dirty, the recursive part
// will not kick in and the dependents won't update. This scenario is not
// common, but it can happen when a solo toggles between an empty group and a
// path for example.
void PathComposer::pathCollapseChanged()
{
    addDirt(ComponentDirt::Path);
    for (auto d : dependents())
    {
        d->addDirt(ComponentDirt::Path, true);
    }
}