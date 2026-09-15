#ifndef _RIVE_CLIPPING_SHAPE_HPP_
#define _RIVE_CLIPPING_SHAPE_HPP_
#include "rive/renderer.hpp"
#include "rive/generated/shapes/clipping_shape_base.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/drawable.hpp"
#ifdef WITH_RIVE_EDITOR
#include "rive/editor/object_arena.hpp"
#endif
#include <vector>

namespace rive
{
class Shape;
class Node;
class RenderPath;
class ClippingShapeStart;
class ClippingShapeEnd;

class ClippingShapeOperation
{
public:
    virtual ~ClippingShapeOperation() = default;
    virtual void draw(Renderer* renderer, bool needsSaveOperation) = 0;
    virtual int emptyClipCount() = 0;
    void clippingShape(ClippingShape* shape) { m_clippingShape = shape; }
    virtual bool isStart() { return false; }
    virtual bool isVisible() { return true; }

protected:
    ClippingShape* m_clippingShape = nullptr;
};

class ClippingShapeStart : public ClippingShapeOperation
{
public:
    void draw(Renderer* renderer, bool needsSaveOperation) override;
    int emptyClipCount() override;
    bool isStart() override { return true; }
    bool isVisible() override;
};

class ClippingShapeEnd : public ClippingShapeOperation
{
    void draw(Renderer* renderer, bool needsSaveOperation) override;
    int emptyClipCount() override;
};

class ClippingShapeProxyDrawable : public Drawable
{
public:
    ClippingShapeProxyDrawable(ClippingShapeOperation* operation) :
        m_clippingShapeOperation(operation)
    {}
    void draw(Renderer* renderer) override
    {
        m_clippingShapeOperation->draw(renderer, m_needsSaveOperation);
    }

    int emptyClipCount() override
    {
        return m_clippingShapeOperation->emptyClipCount();
    }

    bool isHidden() const override { return false; }

    Drawable* hittableComponent() override { return nullptr; };

    bool isTargetOpaque() override { return false; };

    Core* hitTest(HitInfo*, const Mat2D&) override { return nullptr; }

    void operation(ClippingShapeOperation* value)
    {
        m_clippingShapeOperation = value;
    }

    bool isProxy() override { return true; }

    bool isClipStart() override { return m_clippingShapeOperation->isStart(); }
    bool isClipEnd() override { return !m_clippingShapeOperation->isStart(); }
    bool willClip() override { return m_clippingShapeOperation->isVisible(); }

private:
    ClippingShapeOperation* m_clippingShapeOperation = nullptr;
};

class ClippingShape : public ClippingShapeBase
{
private:
    std::vector<Shape*> m_Shapes;
    std::vector<ClippingShapeProxyDrawable*> m_proxyDrawables;
    std::vector<ClippingShapeProxyDrawable*> m_pooledProxyDrawables;
    Node* m_Source = nullptr;
#ifdef WITH_RIVE_EDITOR
    // Slice 6 Phase E dual-storage. See targeted_constraint.hpp.
    CoreHandle m_SourceHandle;
#endif

public:
    ~ClippingShape();
#ifdef WITH_RIVE_EDITOR
    // Body in editor_native/native/src/editor/shapes/
    // clipping_shape_editor.cpp.
    Node* source() const;
    void setSourceForEditor(Node* n);
#else
    inline Node* source() const { return m_Source; }
#endif
    const std::vector<Shape*>& shapes() const { return m_Shapes; }
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;
    void buildDependencies() override;
    void update(ComponentDirt value) override;
    void isVisibleChanged() override;

#ifdef WITH_RIVE_EDITOR
    // Editor-only property-change hooks. Bodies in
    // `editor_native/native/src/editor/shapes/clipping_shape_editor.cpp`.
    // - `sourceIdChanged`: resolves new sourceId to Node, swaps source
    //   storage, clears cached clip path, dirties the clipping pipeline.
    //   The dependent-shape rebuild rides on the next finalizeBatch's
    //   Pass B (which re-runs `buildDependencies` for every Component).
    // - `fillRuleChanged`: dart parity — propagates Clipping dirt down
    //   the parent's subtree (re-clips affected drawables) and Path
    //   dirt on this so the next update recomposes the clip path.
    void sourceIdChanged() override;
    void fillRuleChanged() override;
    // Stage A v2 — typed-list maintenance on add / remove / reparent.
    // `from`'s subtree drawables: removeClippingShapeForEditor(this).
    // `to`'s subtree drawables: addClippingShape(this). Mirrors
    // `onAddedClean`'s walk (which only fires once at hydration).
    // Body in `component_parent_editor.cpp` — same file as the rest
    // of the typed-list editorParentChanged overrides.
    void editorParentChanged(ContainerComponent* from,
                             ContainerComponent* to) override;
#endif

    ShapePaintPath* path() { return m_clipPath; }
    void resetDrawables()
    {
        m_pooledProxyDrawables.insert(m_pooledProxyDrawables.end(),
                                      m_proxyDrawables.begin(),
                                      m_proxyDrawables.end());
        m_proxyDrawables.clear();
    }

    ClippingShapeProxyDrawable* createProxyDrawable(
        ClippingShapeOperation* operation)
    {
        ClippingShapeProxyDrawable* drawable;
        if (m_pooledProxyDrawables.size() > 0)
        {
            drawable = m_pooledProxyDrawables.back();
            drawable->operation(operation);
            drawable->needsSaveOperation(true);
            m_pooledProxyDrawables.pop_back();
        }
        else
        {
            drawable = new ClippingShapeProxyDrawable(operation);
        }
        m_proxyDrawables.push_back(drawable);
        return drawable;
    }
    ClippingShapeStart clipStart;
    ClippingShapeEnd clipEnd;

private:
    // Folds a shape's fills into m_path, preferring post-effect paths. False
    // when it contributed nothing.
    bool addFillPaths(Shape* shape);
    ShapePaintPath m_path;
    ShapePaintPath* m_clipPath = nullptr;
};
} // namespace rive

#endif
