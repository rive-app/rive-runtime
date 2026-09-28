#include "rive/layer_mask.hpp"

#include "rive/artboard.hpp"
#include "rive/drawable.hpp"
#include "rive/node.hpp"
#ifdef RIVE_CANVAS
#include "rive/renderer/render_canvas.hpp"
#endif

using namespace rive;

// Defined out of line so the rcp<gpu::RenderCanvas> members are constructed and
// destroyed in a TU where RenderCanvas is complete.
LayerMask::LayerMask() = default;

LayerMask::~LayerMask()
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

int LayerMaskProxyDrawable::emptyClipCount()
{
    // Only the source bracket suppresses anything, and only while the mask is
    // actually doing something: an invisible or self-referential mask draws its
    // subtree inline, so its source has to keep drawing too or it would simply
    // vanish.
    if (m_op != LayerMaskOp::sourceStart && m_op != LayerMaskOp::sourceEnd)
    {
        return 0;
    }
    if (m_mask->sourceDraws() || !m_mask->isVisible() ||
        m_mask->isSelfReferential())
    {
        return 0;
    }
    return m_op == LayerMaskOp::sourceStart ? 1 : -1;
}

LayerMaskProxyDrawable* LayerMask::createProxyDrawable(LayerMaskOp op)
{
    LayerMaskProxyDrawable* drawable;
    if (!m_pooledProxyDrawables.empty())
    {
        drawable = m_pooledProxyDrawables.back();
        drawable->mask(this);
        drawable->op(op);
        drawable->pairedEnd(nullptr);
        drawable->needsSaveOperation(true);
        m_pooledProxyDrawables.pop_back();
    }
    else
    {
        drawable = new LayerMaskProxyDrawable(this, op);
    }
    m_proxyDrawables.push_back(drawable);
    return drawable;
}

StatusCode LayerMask::onAddedDirty(CoreContext* context)
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
    // Through the setter so the handle is populated at hydration too; source()
    // reads the handle in an editor build and would otherwise see nothing.
    setSourceForEditor(static_cast<Node*>(coreObject));
#else
    m_Source = static_cast<Node*>(coreObject);
#endif
    return StatusCode::Ok;
}

// The masked subtree and the coverage subtree have to be disjoint, in BOTH
// directions.
//
// Source at or below the masked node is the obvious half: the coverage pass
// would re-enter the very range it is rasterizing.
//
// Source ABOVE it is the half that is easy to miss and is worse. The source's
// subtree then contains every masked drawable, so they are coverage as well as
// content: with sourceDraws false the enclosing source bracket suppresses the
// masked range outright and the layer disappears from the normal pass, and with
// sourceDraws true the coverage pass walks straight back into this mask's own
// bracket and the content ends up inside its own coverage.
//
// Shared with the editor's recompute in layer_mask_editor.cpp, because three
// copies of this walk is how it came to be one-directional in all of them.
// Whether `inner` is `container` or sits somewhere beneath it. One direction
// only, unlike subtreesOverlap: "does this mask's region contain that subtree"
// is not symmetric.
bool LayerMask::encloses(Component* container, Component* inner)
{
    if (container == nullptr || inner == nullptr)
    {
        return false;
    }
    for (Component* c = inner; c != nullptr; c = c->parent())
    {
        if (c == container)
        {
            return true;
        }
    }
    return false;
}

bool LayerMask::subtreesOverlap(Component* source, Component* masked)
{
    if (source == nullptr || masked == nullptr)
    {
        return false;
    }
    for (Component* c = source; c != nullptr; c = c->parent())
    {
        if (c == masked)
        {
            return true;
        }
    }
    for (Component* c = masked; c != nullptr; c = c->parent())
    {
        if (c == source)
        {
            return true;
        }
    }
    return false;
}

StatusCode LayerMask::onAddedClean(CoreContext* context)
{
    // Detected before stamping anything, and left inert rather than recursing.
    Node* src = source();
    m_isSelfReferential = subtreesOverlap(src, parent());
    if (src == nullptr || m_isSelfReferential)
    {
        return StatusCode::Ok;
    }

    // Drawables parented (directly or transitively) to this mask's parent are
    // the ones it masks; they need to know so the draw-order sort can bracket
    // the contiguous range they occupy.
    parent()->forAll([this](Component* component) -> bool {
        if (component->is<Drawable>())
        {
            component->as<Drawable>()->addLayerMask(this);
        }
        return true;
    });

    // The source's drawables are collected separately: they live wherever the
    // source node sits in the flat draw list, which is generally nowhere near
    // the masked range, so the coverage pass walks this list instead of a
    // range. Note it deliberately keeps hidden drawables -- suppressing the
    // source in the normal pass is what the bracket does, and the coverage
    // pass still has to draw it.
    src->forAll([this](Component* component) -> bool {
        if (component->is<Drawable>())
        {
            Drawable* drawable = component->as<Drawable>();
            m_SourceDrawables.push_back(drawable);
            // The back-reference, so a change to a source drawable can find
            // the masks whose coverage it feeds. buildDependencies() covers
            // anything that raises dirt; this exists for the one edit that
            // raises none -- see SolidColor::colorValueChanged.
            drawable->addSourceOfLayerMask(this);
        }
        return true;
    });

    return StatusCode::Ok;
}

void LayerMask::markRasterDirty()
{
#ifdef RIVE_CANVAS
    invalidate();
#endif
}

void LayerMask::buildDependencies()
{
#ifdef RIVE_CANVAS
    // A self-referential mask never rasterizes, so it never needs invalidating.
    if (m_isSelfReferential)
    {
        return;
    }
    dependOnSubtree(parent());
    if (Node* src = source())
    {
        dependOnSubtree(src);
    }
#endif
}

#ifdef RIVE_CANVAS
void LayerMask::dependOnSubtree(ContainerComponent* root)
{
    if (root == nullptr)
    {
        return;
    }
    root->forAll([this](Component* component) -> bool {
        // Never ourselves -- forAll over parent() reaches us, and a self-edge
        // is a cycle the sorter refuses to resolve -- and never a sibling mask.
        //
        // A sibling edge would be the honest one: stacked masks nest, and the
        // outer rasterizes the inner one's output. But which is outer is
        // decided by sortDrawOrder, long after this runs, so the edge would
        // have to go both ways, and that is a cycle. invalidate() walks the
        // siblings explicitly instead.
        if (component != this && !component->is<LayerMask>())
        {
            component->addDependent(this);
        }
        return true;
    });
}
#endif

void LayerMask::update(ComponentDirt value)
{
#ifdef RIVE_CANVAS
    // Something we depend on moved, changed shape, or changed colour, so the
    // cached rasters describe a state that no longer exists. Deliberately not
    // filtered by dirt bit: every edge exists because it can change pixels, so
    // any dirt arriving here is reason enough.
    //
    // Through invalidate() rather than setting m_dirty, because there are two
    // more things to undo and both were being missed here. The shrink streak
    // was measuring the old geometry. And stacked masks on one element nest,
    // with the outer rasterizing the inner's composited output, so a change the
    // inner one depends on is a change for the outer one too -- but the graph
    // only delivers dirt to the mask whose own range or source moved, and for
    // masks stacked on the same element those sources differ. The outer kept
    // compositing stale pixels.
    invalidate();
#endif
}

#ifdef RIVE_CANVAS
void LayerMask::invalidate()
{
    // The cached rasters are no longer trustworthy; rebuild on the next draw.
    m_dirty = true;
    // Whatever the shrink policy was part-way through deciding was about the
    // old geometry. Start it over rather than letting a stale streak fire a
    // resize on the first frame after a change.
    m_shrinkStreak = 0;
    // Our composite is somebody else's input, and theirs is cached too.
    //
    // Two ways that happens, and only the first is visible from the hierarchy.
    // Masks stacked on one element nest, the outer rasterizing the inner's
    // output. But a mask on any ANCESTOR also rasterizes our output into its
    // content, and a mask whose SOURCE contains us rasterizes it into its
    // coverage -- and that one can live anywhere in the tree, so no walk from
    // here finds it. buildDependencies cannot carry any of this either: it
    // skips LayerMask deliberately, because the nesting order is not known
    // until sortDrawOrder and the edges would have to run both ways and cycle.
    //
    // So ask the artboard for every mask and keep the ones whose region
    // contains ours. Marked directly rather than by calling their invalidate():
    // this scan already reaches every mask transitively enclosing us, so
    // recursing would only repeat the same work and risk not terminating.
    if (Artboard* owner = artboard())
    {
        ContainerComponent* ours = parent();
        for (LayerMask* other : owner->allLayerMasks())
        {
            if (other == this || other->m_dirty)
            {
                continue;
            }
            if (encloses(other->parent(), ours) ||
                encloses(other->source(), ours))
            {
                other->m_dirty = true;
                other->m_shrinkStreak = 0;
            }
        }
    }
    // ...but only if there *is* a next draw. m_dirty is private, so nothing
    // outside can tell the artboard something changed -- and a host that skips
    // rendering while Artboard::didChange() is false would then never submit
    // the frame that rebuilds them, leaving stale pixels on screen for good.
    if (Artboard* owner = artboard())
    {
        owner->changed();
    }
}

void LayerMask::releaseCanvases()
{
    m_contentCanvas.reset();
    m_maskCanvas.reset();
    m_widthPx = 0;
    m_heightPx = 0;
    // These describe pixels that no longer exist; a composite reading either
    // would place a stale raster.
    m_tightBox = AABB();
    m_box = AABB();
    m_shrinkStreak = 0;
}
#endif

// Moving or resizing the authored rectangle moves the region the rasters cover,
// so whatever is cached no longer belongs where it would be composited. The
// useCustomBounds bit needs no hook of its own: it rides maskFlags, whose
// setter already funnels through maskFlagsChanged().
void LayerMask::boundsXChanged()
{
#ifdef RIVE_CANVAS
    invalidate();
#endif
}

void LayerMask::boundsYChanged()
{
#ifdef RIVE_CANVAS
    invalidate();
#endif
}

void LayerMask::boundsWidthChanged()
{
#ifdef RIVE_CANVAS
    invalidate();
#endif
}

void LayerMask::boundsHeightChanged()
{
#ifdef RIVE_CANVAS
    invalidate();
#endif
}

void LayerMask::resolutionChanged()
{
#ifdef RIVE_CANVAS
    // A new resolution means both cached rasters are the wrong size.
    invalidate();
#endif
}

void LayerMask::maskFlagsChanged()
{
#ifdef RIVE_CANVAS
    invalidate();
    if (!isVisible())
    {
        // Freeing the textures is the point of a disable switch. Safe to drop
        // them mid-frame: the canvases are refcounted and a DeferredFrame being
        // replayed holds its own references.
        releaseCanvases();
    }
#endif
}
