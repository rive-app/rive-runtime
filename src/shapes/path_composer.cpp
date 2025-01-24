#include "rive/shapes/path_composer.hpp"
#include "rive/artboard.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/path.hpp"
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
    if (m_deferredPathDirt)
    {
        // We'd deferred the update, let's make sure the rest of our
        // dependencies update too. Constraints need to update too, stroke
        // effects, etc.
        m_shape->pathChanged();
    }
}

void PathComposer::update(ComponentDirt value)
{
    if (hasDirt(value, ComponentDirt::Path | ComponentDirt::NSlicer))
    {
        if (m_shape->canDeferPathUpdate())
        {
            m_deferredPathDirt = true;
            return;
        }
        m_deferredPathDirt = false;

        if (m_shape->isFlagged(PathFlags::local))
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
        if (m_shape->isFlagged(PathFlags::localClockwise))
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
                    (localTransform.determinant() *
                         (path->as<PointsPath>()->isClockwise() ? 1.0f
                                                                : -1.0f) <
                     0);
                if (isNotClockwise)
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