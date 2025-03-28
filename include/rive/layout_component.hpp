#ifndef _RIVE_LAYOUT_COMPONENT_HPP_
#define _RIVE_LAYOUT_COMPONENT_HPP_
#include "rive/advance_flags.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/drawable.hpp"
#include "rive/generated/layout_component_base.hpp"
#include "rive/layout/layout_measure_mode.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/advancing_component.hpp"
#include "rive/layout/layout_enums.hpp"

namespace rive
{
class AABB;
class KeyFrameInterpolator;
struct LayoutData;
class LayoutComponentStyle;
class LayoutConstraint;
class Layout
{
public:
    Layout() : m_left(0.0f), m_top(0.0f), m_width(0.0f), m_height(0.0f) {}
    Layout(float left, float top, float width, float height) :
        m_left(left), m_top(top), m_width(width), m_height(height)
    {}

    bool operator==(const Layout& o) const
    {
        return m_left == o.m_left && m_top == o.m_top && m_width == o.m_width &&
               m_height == o.m_height;
    }
    bool operator!=(const Layout& o) const { return !(*this == o); }

    static Layout lerp(const Layout& from, const Layout& to, float f)
    {
        float fi = 1.0f - f;
        return Layout(to.m_left * f + from.m_left * fi,
                      to.m_top * f + from.m_top * fi,
                      to.m_width * f + from.m_width * fi,
                      to.m_height * f + from.m_height * fi);
    }

    float left() const { return m_left; }
    float top() const { return m_top; }
    float width() const { return m_width; }
    float height() const { return m_height; }

private:
    float m_left;
    float m_top;
    float m_width;
    float m_height;
};

class LayoutPadding
{
public:
    LayoutPadding() : m_left(0.0f), m_top(0.0f), m_right(0.0f), m_bottom(0.0f)
    {}
    LayoutPadding(float left, float top, float right, float bottom) :
        m_left(left), m_top(top), m_right(right), m_bottom(bottom)
    {}

    bool operator==(const LayoutPadding& o) const
    {
        return m_left == o.m_left && m_top == o.m_top && m_right == o.m_right &&
               m_bottom == o.m_bottom;
    }
    bool operator!=(const LayoutPadding& o) const { return !(*this == o); }

    float left() const { return m_left; }
    float top() const { return m_top; }
    float right() const { return m_right; }
    float bottom() const { return m_bottom; }

private:
    float m_left;
    float m_top;
    float m_right;
    float m_bottom;
};

struct LayoutAnimationData
{
    float elapsedSeconds = 0.0f;
    Layout from;
    Layout to;
    Layout interpolate(float f) const { return Layout::lerp(from, to, f); }
    void copy(const LayoutAnimationData& from);
};

class LayoutComponent : public LayoutComponentBase,
                        public ProxyDrawing,
                        public ShapePaintContainer,
                        public AdvancingComponent,
                        public InterpolatorHost,
                        public LayoutNodeProvider
{
protected:
    LayoutComponentStyle* m_style = nullptr;
    LayoutData* m_layoutData;

    Layout m_layout;
    LayoutPadding m_layoutPadding;

    LayoutAnimationData m_animationDataA;
    LayoutAnimationData m_animationDataB;
    bool m_isSmoothingAnimation = false;
    KeyFrameInterpolator* m_inheritedInterpolator;
    LayoutStyleInterpolation m_inheritedInterpolation =
        LayoutStyleInterpolation::hold;
    float m_inheritedInterpolationTime = 0;
    LayoutDirection m_inheritedDirection = LayoutDirection::inherit;
    Rectangle m_backgroundRect;
    ShapePaintPath m_localPath;
    ShapePaintPath m_worldPath;
    DrawableProxy m_proxy;
    bool m_displayHidden = false;

    Artboard* getArtboard() override { return artboard(); }
    LayoutAnimationData* currentAnimationData();

    LayoutComponent* layoutParent()
    {
        auto p = parent();
        while (p != nullptr)
        {
            if (p->is<LayoutComponent>())
            {
                return p->as<LayoutComponent>();
            }
            p = p->parent();
        }
        return nullptr;
    }
    bool isDisplayHidden() const;
    void propagateCollapse(bool collapse);
    bool collapse(bool value) override;

private:
    float m_widthOverride = NAN;
    int m_widthUnitValueOverride = -1;
    float m_heightOverride = NAN;
    int m_heightUnitValueOverride = -1;
    bool m_parentIsRow = true;
    bool m_widthIntrinsicallySizeOverride = false;
    bool m_heightIntrinsicallySizeOverride = false;
    float m_forcedWidth = NAN;
    float m_forcedHeight = NAN;
    bool m_forceUpdateLayoutBounds = false;

#ifdef WITH_RIVE_LAYOUT
protected:
    void syncLayoutChildren();
    void propagateSizeToChildren(ContainerComponent* component);
    bool applyInterpolation(float elapsedSeconds, bool animate = true);
    void calculateLayout();
    bool styleDisplayHidden();
#endif

public:
    // Implemented for ShapePaintContainer.
    const Mat2D& shapeWorldTransform() const override
    {
        return worldTransform();
    }

    LayoutComponentStyle* style() { return m_style; }
    void style(LayoutComponentStyle* style) { m_style = style; }

    void draw(Renderer* renderer) override;
    void drawProxy(Renderer* renderer) override;
    bool isProxyHidden() override { return isHidden(); }
    Core* hitTest(HitInfo*, const Mat2D&) override;
    DrawableProxy* proxy() { return &m_proxy; };
    virtual void updateRenderPath();
    void update(ComponentDirt value) override;
    void onDirty(ComponentDirt value) override;
    AABB layoutBounds() override
    {
        return AABB::fromLTWH(m_layout.left(),
                              m_layout.top(),
                              m_layout.width(),
                              m_layout.height());
    }
    AABB localBounds() const override
    {
        return AABB::fromLTWH(0.0f, 0.0f, m_layout.width(), m_layout.height());
    }
    AABB worldBounds() const
    {
        auto transform = worldTransform();
        return AABB::fromLTWH(transform[4],
                              transform[5],
                              m_layout.width(),
                              m_layout.height());
    }

    float x() const override { return layoutX(); }
    float y() const override { return layoutY(); }
    float layoutX() const { return m_layout.left(); }
    float layoutY() const { return m_layout.top(); }
    float layoutWidth() { return m_layout.width(); }
    float layoutHeight() { return m_layout.height(); }
    float innerWidth()
    {
        return m_layout.width() - m_layoutPadding.left() -
               m_layoutPadding.right();
    }
    float innerHeight()
    {
        return m_layout.height() - m_layoutPadding.top() -
               m_layoutPadding.bottom();
    }
    float paddingLeft() { return m_layoutPadding.left(); }
    float paddingRight() { return m_layoutPadding.right(); }
    float paddingTop() { return m_layoutPadding.top(); }
    float paddingBottom() { return m_layoutPadding.bottom(); }

    // We provide a way for nested artboards (or other objects) to override this
    // layout's width/height and unit values.
    void widthOverride(float width, int unitValue = 1, bool isRow = true);
    void heightOverride(float height, int unitValue = 1, bool isRow = true);
    void widthIntrinsicallySizeOverride(bool intrinsic);
    void heightIntrinsicallySizeOverride(bool intrinsic);
    virtual bool canHaveOverrides() { return false; }
    bool mainAxisIsRow();
    bool mainAxisIsColumn();
    bool overridesKeyedInterpolation(int propertyKey) override;
    bool hasShapePaints() const { return m_ShapePaints.size() > 0; }
    const Rectangle* backgroundRect() const { return &m_backgroundRect; }
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;
    bool isHidden() const override;
    float forcedWidth() { return m_forcedWidth; }
    float forcedHeight() { return m_forcedHeight; }
    void forcedWidth(float width);
    void forcedHeight(float height);
    void updateConstraints() override;
    TransformComponent* transformComponent() override
    {
        return this->as<TransformComponent>();
    }

    LayoutComponent();
    ~LayoutComponent();
#ifdef WITH_RIVE_LAYOUT

    void syncStyle();
    virtual void propagateSize();
    void updateLayoutBounds(bool animate = true);
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;
    bool advance(float elapsedSeconds);
    bool animates();
    LayoutAnimationStyle animationStyle();
    KeyFrameInterpolator* interpolator();
    LayoutStyleInterpolation interpolation();
    float interpolationTime();

    bool cascadeLayoutStyle(LayoutStyleInterpolation inheritedInterpolation,
                            KeyFrameInterpolator* inheritedInterpolator,
                            float inheritedInterpolationTime,
                            LayoutDirection direction);
    bool setInheritedInterpolation(
        LayoutStyleInterpolation inheritedInterpolation,
        KeyFrameInterpolator* inheritedInterpolator,
        float inheritedInterpolationTime);
    void clearInheritedInterpolation();
    void interruptAnimation();
    bool isLeaf();
    void positionTypeChanged();
    void scaleTypeChanged();
    void displayChanged();
    void flexDirectionChanged();
    void directionChanged();
    LayoutDirection actualDirection();
#endif
    void buildDependencies() override;

    void markLayoutNodeDirty(bool shouldForceUpdateLayoutBounds = false);
    void markLayoutStyleDirty();
    void clipChanged() override;
    void widthChanged() override;
    void heightChanged() override;
    void styleIdChanged() override;
    void fractionalWidthChanged() override;
    void fractionalHeightChanged() override;

    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;

    ShapePaintPath* worldPath() override;
    ShapePaintPath* localPath() override;
    ShapePaintPath* localClockwisePath() override;
    Component* pathBuilder() override;
};
} // namespace rive

#endif