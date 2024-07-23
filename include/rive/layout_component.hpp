#ifndef _RIVE_LAYOUT_COMPONENT_HPP_
#define _RIVE_LAYOUT_COMPONENT_HPP_
#include "rive/drawable.hpp"
#include "rive/generated/layout_component_base.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout/layout_measure_mode.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#ifdef WITH_RIVE_LAYOUT
#include "yoga/YGNode.h"
#include "yoga/YGStyle.h"
#include "yoga/Yoga.h"
#endif
#include <stdio.h>
namespace rive
{

class AABB;
class KeyFrameInterpolator;

struct LayoutData
{
#ifdef WITH_RIVE_LAYOUT
    YGNode node;
    YGStyle style;
#endif
};

struct LayoutAnimationData
{
    float elapsedSeconds = 0;
    AABB fromBounds = AABB();
    AABB toBounds = AABB();
};

class LayoutComponent : public LayoutComponentBase, public ProxyDrawing, public ShapePaintContainer
{
protected:
    LayoutComponentStyle* m_style = nullptr;
    std::unique_ptr<LayoutData> m_layoutData;

    float m_layoutSizeWidth = 0;
    float m_layoutSizeHeight = 0;
    float m_layoutLocationX = 0;
    float m_layoutLocationY = 0;

    LayoutAnimationData m_animationData;
    KeyFrameInterpolator* m_inheritedInterpolator;
    LayoutStyleInterpolation m_inheritedInterpolation = LayoutStyleInterpolation::hold;
    float m_inheritedInterpolationTime = 0;
    Rectangle* m_backgroundRect = new Rectangle();
    rcp<RenderPath> m_backgroundPath;
    rcp<RenderPath> m_clipPath;
    DrawableProxy m_proxy;

    Artboard* getArtboard() override { return artboard(); }

private:
    virtual void performUpdate(ComponentDirt value);

#ifdef WITH_RIVE_LAYOUT
private:
    YGNode& layoutNode() { return m_layoutData->node; }
    YGStyle& layoutStyle() { return m_layoutData->style; }
    void syncLayoutChildren();
    void propagateSizeToChildren(ContainerComponent* component);
    AABB findMaxIntrinsicSize(ContainerComponent* component, AABB maxIntrinsicSize);
    bool applyInterpolation(double elapsedSeconds);

protected:
    void calculateLayout();
#endif

public:
    LayoutComponentStyle* style() { return m_style; }
    void style(LayoutComponentStyle* style) { m_style = style; }
    void draw(Renderer* renderer) override;
    void drawProxy(Renderer* renderer) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    DrawableProxy* proxy() { return &m_proxy; };
    void update(ComponentDirt value) override;

#ifdef WITH_RIVE_LAYOUT
    LayoutComponent() : m_layoutData(std::unique_ptr<LayoutData>(new LayoutData())), m_proxy(this)
    {
        layoutNode().getConfig()->setPointScaleFactor(0);
    }
    ~LayoutComponent() { delete m_backgroundRect; }
    void syncStyle();
    virtual void propagateSize();
    void updateLayoutBounds();
    StatusCode onAddedDirty(CoreContext* context) override;

    bool advance(double elapsedSeconds);
    bool animates();
    LayoutAnimationStyle animationStyle();
    KeyFrameInterpolator* interpolator();
    LayoutStyleInterpolation interpolation();
    float interpolationTime();

    void cascadeAnimationStyle(LayoutStyleInterpolation inheritedInterpolation,
                               KeyFrameInterpolator* inheritedInterpolator,
                               float inheritedInterpolationTime);
    void setInheritedInterpolation(LayoutStyleInterpolation inheritedInterpolation,
                                   KeyFrameInterpolator* inheritedInterpolator,
                                   float inheritedInterpolationTime);
    void clearInheritedInterpolation();
    virtual AABB layoutBounds()
    {
        return AABB(m_layoutLocationX,
                    m_layoutLocationY,
                    m_layoutLocationX + m_layoutSizeWidth,
                    m_layoutLocationY + m_layoutSizeHeight);
    };
    bool hasLayoutMeasurements()
    {
        return m_layoutLocationX != 0 || m_layoutLocationY != 0 || m_layoutSizeWidth != 0 ||
               m_layoutSizeHeight != 0;
    };
#else
    LayoutComponent() : m_layoutData(std::unique_ptr<LayoutData>(new LayoutData())), m_proxy(this)
    {}

#endif
    void buildDependencies() override;

    void markLayoutNodeDirty();
    void markLayoutStyleDirty();
    void clipChanged() override;
    void widthChanged() override;
    void heightChanged() override;
    void styleIdChanged() override;

    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
};
} // namespace rive

#endif