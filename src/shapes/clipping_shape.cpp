#include "rive/shapes/clipping_shape.hpp"
#include "rive/artboard.hpp"
#include "rive/core_context.hpp"
#include "rive/factory.hpp"
#include "rive/node.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/fill.hpp"
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
    // Find drawables parented (directly or transitively) to this clipping
    // shape's parent; they need to know they'll be clipped by this shape.
    parent()->forAll([this](Component* component) -> bool {
        if (component->is<Drawable>())
        {
            component->as<Drawable>()->addClippingShape(this);
        }
        return true;
    });

    // Find shapes parented (directly or transitively) to the source node;
    // their paths will need to be RenderPaths in order to be used for
    // clipping operations.
    if (m_Source)
    {
        m_Source->forAll([this](Component* component) -> bool {
            if (component->is<Shape>())
            {
                auto shape = component->as<Shape>();
                shape->addFlags(PathFlags::world | PathFlags::clipping);
                m_Shapes.push_back(shape);
            }
            return true;
        });
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

#ifdef WITH_RIVE_EDITOR
    setSourceForEditor(static_cast<Node*>(coreObject));
#else
    m_Source = static_cast<Node*>(coreObject);
#endif

    return StatusCode::Ok;
}

void ClippingShape::buildDependencies()
{
#ifdef WITH_RIVE_EDITOR
    // Dart parity: every dep-rebuild clears + re-walks the source
    // subtree. Runtime builds rely on `onAddedClean`'s one-shot init;
    // editor needs to re-walk because `source` can change mid-edit
    // (sourceIdChanged → setSourceForEditor → next Pass B re-runs us).
    // `editorClearDependents` in Pass B-prep wipes stale dependent
    // entries on every Component, so re-adding the pathComposer edges
    // below is safe and required even when m_Shapes contents didn't
    // change.
    m_Shapes.clear();
    if (auto* src = source())
    {
        src->forAll([this](Component* c) -> bool {
            if (c->is<Shape>())
            {
                auto* shape = c->as<Shape>();
                shape->addFlags(PathFlags::world | PathFlags::clipping);
                m_Shapes.push_back(shape);
            }
            return true;
        });
    }
#endif
    for (auto shape : m_Shapes)
    {
        shape->pathComposer()->addDependent(this);
        // We read what a fill resolves in its own update, so we sort after it.
        for (auto paint : shape->shapePaints())
        {
            if (paint->is<Fill>() && paint->hasEffects())
            {
                paint->addDependent(this);
            }
        }
    }
    clipStart.clippingShape(this);
    clipEnd.clippingShape(this);
#ifdef WITH_RIVE_EDITOR
    // Dart parity (clipping_shape.dart:97): force the clip path to
    // rebuild next update cycle. Without this, anything that triggers
    // a Pass B re-run without an accompanying property dirt (e.g. a
    // downstream Fill/Stroke removal that only touches m_ShapePaints)
    // leaves the cached m_path stale or empty — drawables clipped by
    // this ClippingShape render incorrectly until something else
    // dirties the source (moving the source shape rebuilds it).
    addDirt(ComponentDirt::Path);
    // Dirty each source shape's PathComposer directly so its next
    // update rebuilds m_worldPath (consumed by our own update reading
    // `shape->pathComposer()->worldPath()`).
    //
    // We target the PathComposer specifically rather than cascading
    // from the Shape via `addDirt(Path, recurse=true)`. The cascade
    // would traverse the Shape's m_Dependents → PathComposer →
    // PathComposer's m_Dependents — and PathComposer isn't an arena
    // Component, so Pass B-prep's m_Dependents wipe sweep doesn't
    // reach it. Stale ClippingShape* entries from previous batches
    // (where a clip was created then freed via undo/redo) accumulate
    // in pathComposer->m_Dependents and the cascade dereferences
    // them, crashing in the recurse step.
    //
    // The direct addDirt(Path) on pathComposer sets its dirt bit +
    // schedules onComponentDirty via the dependency root, without
    // walking the contaminated dependents list. PathComposer.update
    // then rebuilds m_worldPath in the topological walk, before
    // ClippingShape.update reads it.
    for (auto shape : m_Shapes)
    {
        shape->pathComposer()->addDirt(ComponentDirt::Path);
    }
#endif
}

static Mat2D identity;

// Strokes are skipped: their effect output is a centerline, meaningless filled.
bool ClippingShape::addFillPaths(Shape* shape)
{
    bool addedEffected = false;
    bool needsRawPath = false;
    for (auto paint : shape->shapePaints())
    {
        if (!paint->is<Fill>())
        {
            continue;
        }
        auto effected =
            paint->hasEffects() ? paint->lastEffectPath(paint) : nullptr;
        if (effected == nullptr)
        {
            // No effect: this fill draws the whole shape.
            needsRawPath = true;
            continue;
        }
        if (!effected->empty())
        {
            const Mat2D& world = shape->worldTransform();
            m_path.addPath(effected, effected->isLocal() ? &world : &identity);
        }
        addedEffected = true;
    }

    if (addedEffected && !needsRawPath)
    {
        return true;
    }
    auto path = shape->pathComposer()->worldPath();
    if (path == nullptr)
    {
        return addedEffected;
    }
    m_path.addPath(path, &identity);
    return true;
}

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
            if (!shape->isEmpty() && addFillPaths(shape))
            {
                m_clipPath = &m_path;
            }
        }
    }
}

void ClippingShape::isVisibleChanged()
{
    artboard()->addDirt(ComponentDirt::Clipping);
}