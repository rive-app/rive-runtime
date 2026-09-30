#ifndef _RIVE_LAYER_MASK_HPP_
#define _RIVE_LAYER_MASK_HPP_
#include "rive/generated/layer_mask_base.hpp"
#include "rive/drawable.hpp"
#include "rive/math/aabb.hpp"
#ifdef RIVE_CANVAS
#include "rive/refcnt.hpp"
#endif
#include <cstdint>
#include <vector>

namespace rive
{
class Drawable;
class Node;
#ifdef RIVE_CANVAS
namespace gpu
{
class RenderCanvas;
}
#endif

// How a mask raster becomes a coverage factor. Mirrors the enumValues order in
// dev/defs/layer_mask.json, which is what the serialized maskModeValue indexes
// into, and the Dart MaskMode enum in rive_core/lib/layer_mask.dart.
enum class MaskMode : uint8_t
{
    alpha = 0,
    invertedAlpha = 1,
    luminance = 2,
    invertedLuminance = 3,
};

// A LayerMask child masks the subtree it is parented to with the rendered
// coverage of another subtree.
//
// This is ClippingShape's twin structurally -- a non-drawable Component holding
// a sourceId -- and its opposite at render time. A clip folds its source into a
// path and calls clipPath, which can only ever express geometry. A mask
// rasterizes its source and multiplies the masked content by a factor derived
// from those pixels, which is what buys gradients, image alpha, text and
// feathered edges as mask sources.
//
// The masked subtree has to be flattened before it is combined, because
// per-shape modulation is wrong for overlapping content:
//
//     (A over B) * m  !=  (A*m) over (B*m)   for m < 1 and A.a > 0
//
// so the object owns two offscreen canvases (content and coverage) rather than
// modulating each drawable. Owning them here means deleting the object frees
// the textures, the same arrangement BitmapCache uses.
class LayerMask;

// The four markers a mask splices into the artboard's flat draw list.
//
//   ... maskStart , D1 , D2 , maskEnd ...        <- the masked subtree
//   ... sourceStart , S1 , S2 , sourceEnd ...    <- the mask source subtree
//
// The two ranges are generally nowhere near each other, which is why the source
// needs its own bracket rather than sitting inside the masked one.
enum class LayerMaskOp : uint8_t
{
    maskStart,
    maskEnd,
    sourceStart,
    sourceEnd,
};

// An inert marker. It never draws: the masked range is composited by
// Artboard::drawMasked when the walk reaches a maskStart, and the source range
// is suppressed by reusing the draw loop's existing empty-clip counter rather
// than by any code of its own.
class LayerMaskProxyDrawable : public Drawable
{
public:
    LayerMaskProxyDrawable(LayerMask* mask, LayerMaskOp op) :
        m_mask(mask), m_op(op)
    {}

    void draw(Renderer*) override {}

    // +1 / -1 around the source range so the draw loop skips it, the same
    // mechanism a clipping shape with an empty path uses. This is why
    // suppressing the source needs no new code in the hot loop -- and why it
    // does not touch DrawableFlag::Hidden, which would also (wrongly) suppress
    // the source in the coverage pass and break LayoutComponent's render path.
    int emptyClipCount() override;

    bool isHidden() const override { return false; }
    Drawable* hittableComponent() override { return nullptr; }
    bool isTargetOpaque() override { return false; }
    Core* hitTest(HitInfo*, const Mat2D&) override { return nullptr; }
    bool isProxy() override { return true; }

    bool isMaskStart() override { return m_op == LayerMaskOp::maskStart; }
    bool isMaskEnd() override { return m_op == LayerMaskOp::maskEnd; }

    LayerMask* mask() const { return m_mask; }
    LayerMaskOp op() const { return m_op; }
    void op(LayerMaskOp value) { m_op = value; }
    void mask(LayerMask* value) { m_mask = value; }

    // Only meaningful on a maskStart: the marker that closes its range, so the
    // walk can hand drawMasked the whole span and then skip past it.
    LayerMaskProxyDrawable* pairedEnd() const { return m_pairedEnd; }
    void pairedEnd(LayerMaskProxyDrawable* value) { m_pairedEnd = value; }

private:
    LayerMask* m_mask;
    LayerMaskOp m_op;
    LayerMaskProxyDrawable* m_pairedEnd = nullptr;
};

class LayerMask : public LayerMaskBase
{
    friend class Artboard;

public:
    // Out of line so the rcp<gpu::RenderCanvas> members are only
    // constructed/destroyed where RenderCanvas is a complete type -- otherwise
    // `new LayerMask()` in the core registry would touch an incomplete type.
    LayerMask();
    ~LayerMask() override;

    // A number bound to the mode can carry any value; an unknown one falls
    // back to alpha, as the editor does.
    MaskMode maskMode() const
    {
        return maskModeValue() <= (uint32_t)MaskMode::invertedLuminance
                   ? static_cast<MaskMode>(maskModeValue())
                   : MaskMode::alpha;
    }

#ifdef WITH_RIVE_EDITOR
    // Dual storage, as every other editor-mutable cross-reference has:
    // DrawTarget::m_DrawableHandle, DrawRules::m_ActiveTargetHandle,
    // TargetedConstraint::m_TargetHandle, ClippingShape::m_SourceHandle. A raw
    // Node* into an arena the editor can delete from goes stale; the handle
    // resolves each time instead. Body in layer_mask_editor.cpp.
    Node* source() const;
    void setSourceForEditor(Node* n);
#else
    Node* source() const { return m_Source; }
#endif

    // The author's rectangle, in artboard space -- the same space the raster
    // box itself is in, so it needs no conversion and stays put whatever the
    // masked layer does. Only meaningful when useCustomBounds() is set, and a
    // zero or negative extent is degenerate, which callers read as "measure it
    // instead".
    AABB customBounds() const
    {
        return AABB::fromLTWH(boundsX(),
                              boundsY(),
                              boundsWidth(),
                              boundsHeight());
    }

    // Drawables of the source subtree, in the order they were collected. The
    // mask pass draws these into the coverage canvas; they are deliberately
    // *not* the same set as the masked range, which lives elsewhere in the
    // flat draw list.
    const std::vector<Drawable*>& sourceDrawables() const
    {
        return m_SourceDrawables;
    }

    // True when the source sits inside the subtree this mask applies to (or is
    // that subtree). Rasterizing it would re-enter the range being rasterized,
    // so the mask is dropped rather than recursing forever.
    bool isSelfReferential() const { return m_isSelfReferential; }

    // Whether a mask rooted at `masked` may take its coverage from `source`:
    // the two subtrees must be disjoint in both directions. Static so the
    // editor's recompute shares the one definition.
    static bool subtreesOverlap(Component* source, Component* masked);

    // Whether `inner` is `container` or sits beneath it. Used by invalidate to
    // find the masks whose cached rasters contain this one's output.
    static bool encloses(Component* container, Component* inner);

    // The markers bracketing the source's span in the flat draw list, set by
    // Artboard::interleaveLayerMasks. The coverage pass walks this range rather
    // than sourceDrawables() directly, so clipping shapes nested inside the
    // source still bracket their own drawables.
    LayerMaskProxyDrawable* sourceStart() const { return m_sourceStart; }
    LayerMaskProxyDrawable* sourceEnd() const { return m_sourceEnd; }
    void sourceBracket(LayerMaskProxyDrawable* start,
                       LayerMaskProxyDrawable* end)
    {
        m_sourceStart = start;
        m_sourceEnd = end;
    }

    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;

#ifdef WITH_RIVE_EDITOR
    // onAddedClean registers this mask into two typed lists on other objects --
    // m_layerMasks on every drawable under parent(), m_sourceOfLayerMasks on
    // every drawable under the source -- and runtime only ever does that once,
    // because its hierarchy is immutable. The editor's is not, so both
    // registrations need a symmetric remove or they dangle. Same contract as
    // ClippingShape, which registers the same way and has the same pair of
    // hooks; bodies live beside its in component_parent_editor.cpp.
    //
    // Fires on reparent and on delete (with to == nullptr), and cleans BOTH
    // lists: m_Source is still valid here, so the coverage registration can be
    // withdrawn from the source subtree at the same time as the masked one.
    //
    void editorParentChanged(ContainerComponent* from,
                             ContainerComponent* to) override;
    // Repointing sourceId in the inspector moves which subtree supplies
    // coverage. m_Source is otherwise resolved once in onAddedDirty, so without
    // this the mask keeps rasterizing the subtree it was authored against.
    void sourceIdChanged() override;

private:
    // Re-derives everything that depends on which subtree is masked and which
    // supplies coverage: self-referentiality, both typed-list registrations and
    // m_SourceDrawables. Shared by the two hooks above, because reparenting and
    // retargeting both invalidate exactly this set.
    void rebuildRegistrationsForEditor(ContainerComponent* maskedRoot,
                                       Node* oldSource);

public:
#endif

    // Registers on the dependency graph so a change anywhere in the masked
    // subtree or the source subtree invalidates this mask's cached rasters --
    // and nothing else does. Without it the only signal available at draw time
    // is Artboard::m_didChange, which anything anywhere in the artboard sets.
    void buildDependencies() override;
    void update(ComponentDirt value) override;

    // "Your cached pixels are stale." For the handful of changes that alter
    // what a shape paints without raising any ComponentDirt, so the graph
    // cannot carry the news -- see SolidColor::colorValueChanged.
    void markRasterDirty();

    // Proxy pooling, mirroring ClippingShape: sortDrawOrder runs on every
    // draw-order change and would otherwise churn four allocations per mask.
    void resetDrawables()
    {
        m_pooledProxyDrawables.insert(m_pooledProxyDrawables.end(),
                                      m_proxyDrawables.begin(),
                                      m_proxyDrawables.end());
        m_proxyDrawables.clear();
        // Cleared with the proxies: a re-sort would otherwise leave these
        // pointing at pooled markers that are no longer in the draw list.
        m_sourceStart = nullptr;
        m_sourceEnd = nullptr;
    }
    LayerMaskProxyDrawable* createProxyDrawable(LayerMaskOp op);

#ifdef TESTING
    // How many times this mask has minted a fresh pair of canvases. The point
    // of the bucketing and the shrink hysteresis is that this stays put while
    // content moves, and a size comparison cannot prove that on its own -- a
    // re-allocation to the same dimensions would be invisible.
    int testAllocations = 0;
    // The raster geometry the last draw settled on. Exposed because the point
    // of sizing from content rather than from the artboard is a smaller,
    // sharper raster, and neither is observable from the recorded command
    // stream -- canvasContentBegin says a canvas was opened, not how big it is.
    uint32_t testWidthPx() const { return m_widthPx; }
    uint32_t testHeightPx() const { return m_heightPx; }
    float testRasterScale() const { return m_rasterScale; }
    // Whether the next draw will re-rasterize. The canvas-open count proves a
    // rebuild happened but not which mask decided to, and invalidation that has
    // to reach masks the hierarchy cannot walk to needs the direct answer.
    bool testDirty() const { return m_dirty; }
    AABB testBox() const { return m_box; }
#endif

protected:
    void resolutionChanged() override;
    // One per edge: the rectangle moves the region the rasters cover, so any of
    // them makes the cached pixels wrong.
    void boundsXChanged() override;
    void boundsYChanged() override;
    void boundsWidthChanged() override;
    void boundsHeightChanged() override;
    // One hook covers all three bits: every passthrough write from CoreRegistry
    // funnels through the maskFlags() setter.
    void maskFlagsChanged() override;

private:
#ifdef RIVE_CANVAS
    // Adds this mask as a dependent of every component under `root`, so any
    // dirt raised in there reaches update(). Deliberately every component
    // rather than a curated set: a mask rasterizes pixels, so a colour, a
    // gradient stop, a trim path and a bone all matter, and enumerating them is
    // an audit that would silently rot. Other LayerMasks are skipped -- see the
    // note on the implementation.
    void dependOnSubtree(ContainerComponent* root);
    // Marks both rasters stale and tells the artboard it changed, so a host
    // gated on Artboard::didChange() still draws the frame that rebuilds them.
    void invalidate();
    void releaseCanvases();
#endif

    Node* m_Source = nullptr;
    std::vector<Drawable*> m_SourceDrawables;
    std::vector<LayerMaskProxyDrawable*> m_proxyDrawables;
    std::vector<LayerMaskProxyDrawable*> m_pooledProxyDrawables;
    LayerMaskProxyDrawable* m_sourceStart = nullptr;
    LayerMaskProxyDrawable* m_sourceEnd = nullptr;
    bool m_isSelfReferential = false;
#ifdef RIVE_CANVAS
    // Set while this mask's own draw is on the stack. onAddedClean catches a
    // source under the mask's OWN parent, but two masks can name each other's
    // subtrees and neither is self-referential by that test: A's source is not
    // under A's parent, it is under B's. Such a cycle is only visible here,
    // because the coverage pass draws a RANGE and drawDrawableRange dispatches
    // to drawMasked on any bracket inside it. Per mask, so legitimate nesting
    // -- a different mask inside a masked subtree -- is unaffected.
    bool m_isDrawing = false;
#endif
#ifdef WITH_RIVE_EDITOR
    CoreHandle m_SourceHandle;
#endif

#ifdef RIVE_CANVAS
    // Both canvases share one size and raster scale so a content texel and a
    // coverage texel at the same coordinate correspond with no resampling.
    rcp<gpu::RenderCanvas> m_contentCanvas;
    rcp<gpu::RenderCanvas> m_maskCanvas;
    uint32_t m_widthPx = 0;
    uint32_t m_heightPx = 0;
    float m_rasterScale = 1.0f;
    bool m_dirty = true;
    // The measured box the content occupies, before the guard band and the size
    // bucketing are applied. Stored because a frame where nothing changed must
    // not re-measure, and re-deriving it from m_box would feed the guard band
    // back through itself -- growing the raster a few texels every frame.
    AABB m_tightBox;
    // The local box the cached pixels actually cover: m_tightBox after the
    // guard band, the texel snap and the bucketing. Unlike the artboard box
    // this used to always be, it moves as the content moves -- so a frame that
    // reuses the cached rasters has to composite them where they were
    // rasterized, not where this frame's plan happens to land.
    AABB m_box;
    // Consecutive draws that wanted a smaller raster. Growth is immediate;
    // shrinking waits, because content animating through a size would otherwise
    // re-allocate two GPU textures every frame.
    uint8_t m_shrinkStreak = 0;
#endif
};
} // namespace rive

#endif
