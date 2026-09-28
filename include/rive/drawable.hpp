#ifndef _RIVE_DRAWABLE_HPP_
#define _RIVE_DRAWABLE_HPP_
#include "rive/generated/drawable_base.hpp"
#include "rive/hit_info.hpp"
#include "rive/lazy_vector.hpp"
#include "rive/renderer.hpp"
#include "rive/clip_result.hpp"
#include "rive/math/aabb.hpp"
#include "rive/drawable_flag.hpp"
#include "rive/shapes/paint/blend_mode.hpp"
#include <vector>

namespace rive
{
class ClippingShape;
class CustomProperty;
class LayerMask;
class Artboard;
class DrawRules;
class LayoutComponent;
class ShapePaintContainer;

// How much to trust a drawable's answer to paintedWorldBounds().
//
// Anything sizing a surface from content bounds needs this: a box that is too
// small crops, and a crop at a raster edge is a hard line across the middle of
// someone's artwork. So a drawable that cannot honestly bound itself has to say
// so rather than return its best guess.
enum class BoundsFidelity : uint8_t
{
    // Cannot measure at all. The caller must fall back to something it knows
    // covers everything, such as the artboard's own bounds.
    none,

    // The footprint is right but the paint's full reach is not known -- glyph
    // ink escaping a text layout box, a stroke effect that can displace
    // geometry. The caller should pad before using it.
    approximate,

    // Geometry plus every stroke, join, cap and feather outset is accounted
    // for.
    exact,
};

class Drawable : public DrawableBase
{
    friend class Artboard;
    friend class StateMachineInstance;

private:
    LazyVector<ClippingShape*> m_ClippingShapes;
    // Kept separate from m_ClippingShapes rather than unified into one
    // bracket list: the clip interleave in Artboard::sortDrawOrder and its
    // goldens are load-bearing, and a mask bracket has different nesting
    // rules (it must sit inside a clip, and it must hug real drawables).
    LazyVector<LayerMask*> m_layerMasks;
    // The other direction: masks that rasterize this drawable as coverage.
    // Disjoint from m_layerMasks -- a mask cannot mask its own source -- and
    // needed because a change to a source drawable has to invalidate the
    // coverage canvas, and the only party that can be asked is the drawable.
    LazyVector<LayerMask*> m_sourceOfLayerMasks;

    /// Used exclusively by the artboard;
    DrawRules* flattenedDrawRules = nullptr;
    Drawable* prev = nullptr;
    Drawable* next = nullptr;

    enum class RuntimeFlags : uint8_t
    {
        none = 0,
        needsSaveOperation = 1 << 0,
        // Set by a property so untagged drawables pay no child scan.
        hasCustomProperties = 1 << 1,
    };
    RuntimeFlags m_runtimeFlags = RuntimeFlags::needsSaveOperation;

    bool runtimeFlag(RuntimeFlags flag) const
    {
        return (m_runtimeFlags & flag) == flag;
    }
    void runtimeFlag(RuntimeFlags flag, bool value)
    {
        m_runtimeFlags = value ? m_runtimeFlags | flag : m_runtimeFlags & ~flag;
    }

protected:
    bool needsSaveOperation() const
    {
        return runtimeFlag(RuntimeFlags::needsSaveOperation);
    }
#ifdef WITH_RIVE_EDITOR
    void flagsChanged() override
    {
        // Preserve runtime-only bits when authoring the shared component flag.
        constexpr auto mask = static_cast<uint16_t>(DrawableFlag::Selectable);
        drawableFlags((drawableFlags() & ~mask) | (flags() & mask));
    }
#endif

public:
    bool isSelectable() const
    {
        return (static_cast<DrawableFlag>(drawableFlags()) &
                DrawableFlag::Selectable) != DrawableFlag::None;
    }

    BlendMode blendMode() const { return (BlendMode)blendModeValue(); }

    /// additiveAmount() as the renderer wants it: 0 (plain srcOver) to 1
    /// (fully additive), and always 0 for the modes that ignore it.
    float additiveness() const
    {
        return additivenessFor(blendMode(), additiveAmount());
    }
    virtual void draw(Renderer* renderer) = 0;
    virtual Core* hitTest(HitInfo*, const Mat2D&) = 0;
    bool hitTestPoint(const Vec2D& position,
                      bool skipOnUnclipped,
                      bool isPrimaryHit) override;
    void addClippingShape(ClippingShape* shape);
    inline const std::vector<ClippingShape*>& clippingShapes() const
    {
        return m_ClippingShapes.view();
    }
    void addLayerMask(LayerMask* mask);
    void addSourceOfLayerMask(LayerMask* mask);
    // The masks that apply to this drawable, outermost ancestor first (they are
    // stamped in m_Objects order, which is hierarchy order). Virtual because
    // DrawableProxy has to answer for the component it stands in for: a
    // LayoutComponent's proxy carries the save()+clipPath() whose restore()
    // lives in LayoutComponent::draw(), so leaving the proxy out of the bracket
    // would strand the save outside it and underflow the offscreen renderer's
    // save stack.
    virtual const std::vector<LayerMask*>& layerMasks() const
    {
        return m_layerMasks.view();
    }
    // The masks this drawable supplies coverage for. Virtual for the same
    // reason layerMasks() is: a proxy answers for what it stands in for.
    virtual const std::vector<LayerMask*>& sourceOfLayerMasks() const
    {
        return m_sourceOfLayerMasks.view();
    }
#ifdef WITH_RIVE_EDITOR
    // Editor-only removal companion to `addClippingShape`. Fires from
    // `ClippingShape::editorParentChanged` when a ClippingShape is
    // reparented away from this drawable's clip-affecting ancestor (or
    // about to be freed) so the typed list doesn't dangle. Runtime
    // assumes immutable hierarchy and never needs this. Body in
    // `editor_native/.../component_parent_editor.cpp`.
    void removeClippingShapeForEditor(ClippingShape* shape);
    // The same companions for the two LayerMask lists, which onAddedClean
    // registers the same one-shot way: m_layerMasks from the mask's parent
    // subtree, m_sourceOfLayerMasks from its source subtree. Without these a
    // mask deleted or reparented in the editor leaves a freed pointer that
    // sortDrawOrder or a colour edit will dereference.
    void removeLayerMaskForEditor(LayerMask* mask);
    void removeSourceOfLayerMaskForEditor(LayerMask* mask);
#endif

    virtual bool isHidden() const
    {
        if ((static_cast<DrawableFlag>(drawableFlags()) &
             DrawableFlag::Hidden) == DrawableFlag::Hidden ||
            hasDirt(ComponentDirt::Collapsed))
        {
            return true;
        }
#ifdef WITH_RIVE_EDITOR
        // Editor-only: respect the hierarchy panel's eye toggle.
        // Walks the parent chain since `ComponentFlags::hidden`
        // doesn't physically cascade to children. Mirrors dart's
        // [stage_hideable.dart:76-80](packages/editor/lib/rive/stage/stage_hideable.dart#L76-L80).
        // Runtime flags() is always 0, so this branch is dead weight
        // there but free at runtime build time.
        if (isFlaggedRecursive(EditorFlagHidden))
        {
            return true;
        }
        // Isolation: when any component in the artboard has the
        // isolated bit set, everything outside the isolated subtree
        // is hidden on stage. After-Effects-style.
        if (isOutsideEditorIsolationScope())
        {
            return true;
        }
#endif
        return false;
    }

#ifdef WITH_RIVE_EDITOR
    /// True iff the active artboard has at least one component with
    /// `EditorFlagIsolated` set AND this drawable is NOT a descendant
    /// of one of those isolated subtrees. Walks the artboard's full
    /// hierarchy on each call — cheap because flagged components are
    /// rare and the walk short-circuits on the first match. Mirrors
    /// dart's [Artboard.hasIsolatedComponents]/[isInIsolationScope]
    /// pair used by
    /// [stage_hideable.dart:87-92](packages/editor/lib/rive/stage/stage_hideable.dart#L87-L92).
    /// Body in `editor_native/native/src/editor/drawable_editor.cpp`
    /// so this header doesn't depend on the full Artboard def.
    bool isOutsideEditorIsolationScope() const;
#endif

    virtual bool isTargetOpaque()
    {
        return (static_cast<DrawableFlag>(drawableFlags()) &
                DrawableFlag::Opaque) == DrawableFlag::Opaque;
    }

    virtual bool isProxy() { return false; }
    virtual bool isClipStart() { return false; }
    virtual bool isClipEnd() { return false; }
    // Opens / closes a layer-mask bracket. Unlike a clip bracket these are not
    // drawn in place: the walk hands the whole enclosed range to
    // Artboard::drawMasked and then skips past it.
    virtual bool isMaskStart() { return false; }
    virtual bool isMaskEnd() { return false; }
    virtual bool willClip() { return false; }
    virtual bool willDraw();

    // World-space bounds of everything this drawable actually paints: geometry
    // plus stroke, join, cap and feather outset. NOT the same as
    // Shape::worldBounds(), which is geometry only and would crop a feather.
    //
    // Defaults to `none` so a Drawable added later is safe by construction --
    // the day someone adds a drawable that paints somewhere unpredictable,
    // every caller degrades instead of silently cropping it. Override only
    // where the answer is genuinely conservative. *out is untouched when `none`
    // is returned.
    virtual BoundsFidelity paintedWorldBounds(AABB* out)
    {
        return BoundsFidelity::none;
    }

protected:
    // The shape every override below takes: lift a local-space box into world
    // space, then pad it by `paints` (null for a drawable that has none).
    //
    // Answers `none` for an empty local box, because
    // TransformComponent::localBounds() returns an empty AABB to mean "I do not
    // know my size" -- and a caller must not read that as "I paint nothing".
    BoundsFidelity paintedBoundsFromLocal(const AABB& local,
                                          const Mat2D& world,
                                          const ShapePaintContainer* paints,
                                          AABB* out);

public:
    void needsSaveOperation(bool value)
    {
        runtimeFlag(RuntimeFlags::needsSaveOperation, value);
    }

    bool isChildOfLayout(LayoutComponent* layout);

    StatusCode onAddedDirty(CoreContext* context) override;

    virtual Drawable* hittableComponent() { return this; }

    // Whether custom properties tag this drawable for a draw visitor.
    bool hasCustomProperties();
    void markHasCustomProperties()
    {
        runtimeFlag(RuntimeFlags::hasCustomProperties, true);
    }
    // The tagging property with this name id, null when there is none.
    CustomProperty* customProperty(uint32_t nameId);

    virtual int emptyClipCount() { return 0; }

    // Public accessors for the artboard's draw-order linked list.
    // Order is back-to-front (firstDrawable() is bottom-most).
    Drawable* nextDrawable() const { return next; }
    Drawable* prevDrawable() const { return prev; }
};

class ProxyDrawing
{
public:
    virtual void drawProxy(Renderer* renderer) = 0;
    virtual bool isProxyHidden() = 0;
};

class DrawableProxy : public Drawable
{
private:
    ProxyDrawing* m_proxyDrawing;

public:
    DrawableProxy(ProxyDrawing* proxy) : m_proxyDrawing(proxy) {}

    void draw(Renderer* renderer) override
    {
        m_proxyDrawing->drawProxy(renderer);
    }

    bool isHidden() const override { return m_proxyDrawing->isProxyHidden(); }

    Drawable* hittableComponent() override;

    bool isTargetOpaque() override;

    Core* hitTest(HitInfo*, const Mat2D&) override { return nullptr; }

    bool isProxy() override { return true; }

    // Delegated for the same reason as layerMasks(): the proxy paints on behalf
    // of another component, so that component's reach is the answer.
    BoundsFidelity paintedWorldBounds(AABB* out) override;

    // A proxy stands in for a real component, so it has to be inside the same
    // mask bracket that component is in. A LayoutComponent's proxy issues
    // save() + clipPath() whose matching restore() is in
    // LayoutComponent::draw(); if the proxy were excluded from the bracket the
    // save would land outside the offscreen frame and the restore inside it.
    const std::vector<LayerMask*>& layerMasks() const override;
    const std::vector<LayerMask*>& sourceOfLayerMasks() const override;

    ProxyDrawing* proxyDrawing() const { return m_proxyDrawing; }
};
} // namespace rive

#endif
