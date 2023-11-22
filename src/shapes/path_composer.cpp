#include "rive/shapes/path_composer.hpp"
#include "rive/artboard.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/shape.hpp"

using namespace rive;

PathComposer::PathComposer(Shape* shape) : m_Shape(shape), m_deferredPathDirt(false) {}

void PathComposer::buildDependencies()
{
    assert(m_Shape != nullptr);
    m_Shape->addDependent(this);
    for (auto path : m_Shape->paths())
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
        m_Shape->pathChanged();
    }
}

void PathComposer::update(ComponentDirt value)
{
    if (hasDirt(value, ComponentDirt::Path))
    {
        if (m_Shape->canDeferPathUpdate())
        {
            m_deferredPathDirt = true;
            return;
        }
        m_deferredPathDirt = false;

        auto space = m_Shape->pathSpace();
        bool hasConstraint = (space & PathSpace::FollowPath) == PathSpace::FollowPath;
        if ((space & PathSpace::Local) == PathSpace::Local)
        {
            if (m_LocalPath == nullptr)
            {
                PathSpace localSpace =
                    (hasConstraint) ? PathSpace::Local & PathSpace::FollowPath : PathSpace::Local;
                m_LocalPath = m_Shape->makeCommandPath(localSpace);
            }
            else
            {
                m_LocalPath->rewind();
            }
            auto world = m_Shape->worldTransform();
            Mat2D inverseWorld = world.invertOrIdentity();
            // Get all the paths into local shape space.
            for (auto path : m_Shape->paths())
            {
                if (!path->isHidden() && !path->isCollapsed())
                {
                    const auto localTransform = inverseWorld * path->pathTransform();
                    m_LocalPath->addPath(path->commandPath(), localTransform);
                }
            }
        }
        if ((space & PathSpace::World) == PathSpace::World)
        {
            if (m_WorldPath == nullptr)
            {
                PathSpace worldSpace =
                    (hasConstraint) ? PathSpace::World & PathSpace::FollowPath : PathSpace::World;
                m_WorldPath = m_Shape->makeCommandPath(worldSpace);
            }
            else
            {
                m_WorldPath->rewind();
            }
            for (auto path : m_Shape->paths())
            {
                if (!path->isHidden() && !path->isCollapsed())
                {
                    const Mat2D& transform = path->pathTransform();
                    m_WorldPath->addPath(path->commandPath(), transform);
                }
            }
        }
    }
}

// Instead of adding dirt and rely on the recursive behavior of the addDirt method,
// we need to explicitly add dirt to the dependents. The reason is that a collapsed
// shape will not clear its dirty path flag in the current frame since it is collapsed.
// So in a future frame if it is uncollapsed, we mark its path flag as dirty again,
// but since it was already dirty, the recursive part will not kick in and the dependents
// won't update.
// This scenario is not common, but it can happen when a solo toggles between an empty
// group and a path for example.
void PathComposer::pathCollapseChanged()
{
    addDirt(ComponentDirt::Path);
    for (auto d : dependents())
    {
        d->addDirt(ComponentDirt::Path, true);
    }
}