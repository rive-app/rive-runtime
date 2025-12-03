#include "rive/shapes/clipping_shape.hpp"
#include "rive/artboard.hpp"
#include "rive/core_context.hpp"
#include "rive/factory.hpp"
#include "rive/node.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/path_composer.hpp"
#include "rive/shapes/shape.hpp"

using namespace rive;

void ClippingShapeStart::draw(Renderer* renderer, bool needsSaveOperation)
{
    if (!m_clippingShape->isVisible())
    {
        return;
    }
    if (needsSaveOperation)
    {

        renderer->save();
    }
    if (m_clippingShape)
    {
        ShapePaintPath* path = m_clippingShape->path();
        if (!path)
        {
            return;
        }
        RenderPath* renderPath = path->renderPath(m_clippingShape);
        renderer->clipPath(renderPath);
    }
}

int ClippingShapeStart::emptyClipCount()
{
    if (m_clippingShape && m_clippingShape->isVisible())
    {
        ShapePaintPath* path = m_clippingShape->path();
        if (!path)
        {
            return 1;
        }
    }
    return 0;
}

bool ClippingShapeStart::isVisible()
{
    if (m_clippingShape)
    {
        return m_clippingShape->isVisible();
    }
    return false;
}

int ClippingShapeEnd::emptyClipCount()
{
    if (m_clippingShape && m_clippingShape->isVisible())
    {
        ShapePaintPath* path = m_clippingShape->path();
        if (!path)
        {
            return -1;
        }
    }
    return 0;
}

void ClippingShapeEnd::draw(Renderer* renderer, bool needsSaveOperation)
{
    if (!m_clippingShape->isVisible() || !needsSaveOperation)
    {
        return;
    }
    renderer->restore();
}

ClippingShape::~ClippingShape()
{
    for (auto& proxy : m_proxyDrawables)
    {
        delete proxy;
    }
    for (auto& proxy : m_pooledProxyDrawables)
    {
        delete proxy;
    }
}

StatusCode ClippingShape::onAddedClean(CoreContext* context)
{
    auto clippingHolder = parent();

    auto artboard = static_cast<Artboard*>(context);
    for (auto core : artboard->objects())
    {
        if (core == nullptr)
        {
            continue;
        }
        // Iterate artboard to find drawables that are parented to this clipping
        // shape, they need to know they'll be clipped by this shape.
        if (core->is<Drawable>())
        {
            auto drawable = core->as<Drawable>();
            for (ContainerComponent* component = drawable; component != nullptr;
                 component = component->parent())
            {
                if (component == clippingHolder)
                {
                    drawable->addClippingShape(this);
                    break;
                }
            }
        }

        // Iterate artboard to find shapes that are parented to the source,
        // their paths will need to be RenderPaths in order to be used for
        // clipping operations.
        if (core->is<Shape>())
        {
            auto component = core->as<ContainerComponent>();
            while (component != nullptr)
            {
                if (component == m_Source)
                {
                    auto shape = core->as<Shape>();
                    shape->addFlags(PathFlags::world | PathFlags::clipping);
                    m_Shapes.push_back(shape);
                    break;
                }
                component = component->parent();
            }
        }
    }

    return StatusCode::Ok;
}

StatusCode ClippingShape::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    auto coreObject = context->resolve(sourceId());
    if (coreObject == nullptr || !coreObject->is<Node>())
    {
        return StatusCode::MissingObject;
    }

    m_Source = static_cast<Node*>(coreObject);

    return StatusCode::Ok;
}

void ClippingShape::buildDependencies()
{
    for (auto shape : m_Shapes)
    {
        shape->pathComposer()->addDependent(this);
    }
    clipStart.clippingShape(this);
    clipEnd.clippingShape(this);
}

static Mat2D identity;
void ClippingShape::update(ComponentDirt value)
{
    if (hasDirt(value,
                ComponentDirt::Path | ComponentDirt::WorldTransform |
                    ComponentDirt::NSlicer))
    {
        m_path.rewind(false, (FillRule)fillRule());
        m_clipPath = nullptr;
        for (auto shape : m_Shapes)
        {
            if (!shape->isEmpty())
            {
                auto path = shape->pathComposer()->worldPath();
                if (path == nullptr)
                {
                    continue;
                }
                m_path.addPath(path, &identity);
                m_clipPath = &m_path;
            }
        }
    }
}
