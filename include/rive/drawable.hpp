#ifndef _RIVE_DRAWABLE_HPP_
#define _RIVE_DRAWABLE_HPP_
#include "rive/generated/drawable_base.hpp"
#include "rive/hit_info.hpp"
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
    std::vector<ClippingShape*> m_ClippingShapes;

    /// Used exclusively by the artboard;
    DrawRules* flattenedDrawRules = nullptr;
    Drawable* prev = nullptr;
    Drawable* next = nullptr;

public:
    BlendMode blendMode() const { return (BlendMode)blendModeValue(); }
    ClipResult applyClip(Renderer* renderer) const;
    virtual void draw(Renderer* renderer) = 0;
    virtual Core* hitTest(HitInfo*, const Mat2D&) = 0;
    void addClippingShape(ClippingShape* shape);
    inline const std::vector<ClippingShape*>& clippingShapes() const { return m_ClippingShapes; }

    inline bool isHidden() const
    {
        return (static_cast<DrawableFlag>(drawableFlags()) & DrawableFlag::Hidden) ==
                   DrawableFlag::Hidden ||
               hasDirt(ComponentDirt::Collapsed);
    }

    inline bool isTargetOpaque() const
    {
        return (static_cast<DrawableFlag>(drawableFlags()) & DrawableFlag::Opaque) ==
               DrawableFlag::Opaque;
    }

    bool isChildOfLayout(LayoutComponent* layout);

    StatusCode onAddedDirty(CoreContext* context) override;
};

class ProxyDrawing
{
public:
    virtual void drawProxy(Renderer* renderer) = 0;
};

class DrawableProxy : public Drawable
{
private:
    ProxyDrawing* m_proxyDrawing;

public:
    DrawableProxy(ProxyDrawing* proxy) : m_proxyDrawing(proxy) {}

    void draw(Renderer* renderer) override { m_proxyDrawing->drawProxy(renderer); }

    Core* hitTest(HitInfo*, const Mat2D&) override { return nullptr; }
};
} // namespace rive

#endif
