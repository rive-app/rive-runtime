#ifndef _RIVE_CLIPPING_SHAPE_HPP_
#define _RIVE_CLIPPING_SHAPE_HPP_
#include "rive/renderer.hpp"
#include "rive/generated/shapes/clipping_shape_base.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/drawable.hpp"
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

public:
    ~ClippingShape();
    Node* source() const { return m_Source; }
    const std::vector<Shape*>& shapes() const { return m_Shapes; }
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;
    void buildDependencies() override;
    void update(ComponentDirt value) override;
    void isVisibleChanged() override;

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
    ShapePaintPath m_path;
    ShapePaintPath* m_clipPath = nullptr;
};
} // namespace rive

#endif
