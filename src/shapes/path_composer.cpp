#include "rive/shapes/path_composer.hpp"
#include "rive/artboard.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/factory.hpp"

using namespace rive;

PathComposer::PathComposer(Shape* shape) : m_shape(shape), m_deferredPathDirt(false) {}

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
    if (hasDirt(value, ComponentDirt::Path))
    {
        if (m_shape->canDeferPathUpdate())
        {
            m_deferredPathDirt = true;
            return;
        }
        m_deferredPathDirt = false;

        if (m_shape->isFlagged(PathFlags::local))
        {
            if (m_localPath == nullptr)
            {
                m_localPath = artboard()->factory()->makeEmptyRenderPath();
            }
            else
            {
                m_localPath->rewind();
                m_localRawPath.rewind();
            }
            auto world = m_shape->worldTransform();
            Mat2D inverseWorld = world.invertOrIdentity();
            // Get all the paths into local shape space.
            for (auto path : m_shape->paths())
            {
                if (!path->isHidden() && !path->isCollapsed())
                {
                    const auto localTransform = inverseWorld * path->pathTransform();
                    m_localRawPath.addPath(path->rawPath(), &localTransform);
                }
            }

            // TODO: add a CommandPath::copy(RawPath)
            m_localRawPath.addTo(m_localPath.get());
        }
        if (m_shape->isFlagged(PathFlags::world))
        {
            if (m_worldPath == nullptr)
            {
                m_worldPath = artboard()->factory()->makeEmptyRenderPath();
            }
            else
            {
                m_worldPath->rewind();
                m_worldRawPath.rewind();
            }
            for (auto path : m_shape->paths())
            {
                if (!path->isHidden() && !path->isCollapsed())
                {
                    const Mat2D& transform = path->pathTransform();
                    m_worldRawPath.addPath(path->rawPath(), &transform);
                }
            }
            // TODO: add a CommandPath::copy(RawPath)
            m_worldRawPath.addTo(m_worldPath.get());
        }
        m_shape->markBoundsDirty();
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