#ifndef _RIVE_DRAWABLE_HPP_
#define _RIVE_DRAWABLE_HPP_
#include "rive/generated/drawable_base.hpp"
#include "rive/hit_info.hpp"
#include "rive/lazy_vector.hpp"
#include "rive/renderer.hpp"
#include "rive/clip_result.hpp"
#include "rive/drawable_flag.hpp"
#include <vector>

namespace rive
{
class ClippingShape;
class Artboard;
class DrawRules;
class LayoutComponent;

class Drawable : public DrawableBase
{
    friend class Artboard;
    friend class StateMachineInstance;

private:
    LazyVector<ClippingShape*> m_ClippingShapes;

    /// Used exclusively by the artboard;
    DrawRules* flattenedDrawRules = nullptr;
    Drawable* prev = nullptr;
    Drawable* next = nullptr;

protected:
    bool m_needsSaveOperation = true;

public:
    BlendMode blendMode() const { return (BlendMode)blendModeValue(); }
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
#ifdef WITH_RIVE_EDITOR
    // Editor-only removal companion to `addClippingShape`. Fires from
    // `ClippingShape::editorParentChanged` when a ClippingShape is
    // reparented away from this drawable's clip-affecting ancestor (or
    // about to be freed) so the typed list doesn't dangle. Runtime
    // assumes immutable hierarchy and never needs this. Body in
    // `editor_native/.../component_parent_editor.cpp`.
    void removeClippingShapeForEditor(ClippingShape* shape);
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
    virtual bool willClip() { return false; }
    virtual bool willDraw();
    void needsSaveOperation(bool value) { m_needsSaveOperation = value; }

    bool isChildOfLayout(LayoutComponent* layout);

    StatusCode onAddedDirty(CoreContext* context) override;

    virtual Drawable* hittableComponent() { return this; }

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

    ProxyDrawing* proxyDrawing() const { return m_proxyDrawing; }
};
} // namespace rive

#endif
