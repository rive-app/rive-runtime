#ifndef _RIVE_LAYOUT_COMPONENT_HPP_
#define _RIVE_LAYOUT_COMPONENT_HPP_
#include "rive/advance_flags.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/drawable.hpp"
#include "rive/generated/layout_component_base.hpp"
#include "rive/layout/layout_measure_mode.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/layout/layout_style_applier.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/advancing_component.hpp"
#include "rive/layout/layout_enums.hpp"
#include <memory>

namespace rive
{
class AABB;
class KeyFrameInterpolator;
class LayoutData;
class LayoutComponentStyle;
class LayoutConstraint;
class ComponentOrigin;

// Containers transparent to layout provide no layout node and no sizing of
// their own, so the layout above descends through them. A plain group exposes
// all its children; a Solo exposes only its active one. Anything else
// (n-slicers, nested artboards, shapes) is opaque.
bool isTransparentLayoutContainer(Component* component);

// Whether a provider reached *through* a transparent container still joins the
// layout. An ArtboardComponentList provides a layout node unconditionally — it
// never opted in the way a participant does — so a group between one and its
// layout is how a file asks for free-form items placed by x/y or a follow-path
// constraint. It joins only when explicitly flagged.
bool joinsLayoutThroughContainer(Component* component);

// Boolean state of a LayoutComponent, packed into one word instead of a dozen
// separate bytes. Gains the bitwise operators + rive::enums helpers for free by
// having a None == 0 member (see rive/enums.hpp).
enum class LayoutComponentFlags : uint16_t
{
    None = 0,
    ParentIsRow = 1 << 0,   // default ON
    ParentIsStack = 1 << 1, //
    WidthIntrinsicallySizeOverride = 1 << 2,
    HeightIntrinsicallySizeOverride = 1 << 3,
    ForceUpdateLayoutBounds = 1 << 4,
    PositionLeftChanged = 1 << 5, // default ON
    PositionTopChanged = 1 << 6,  // default ON
    HasForegroundDrawable = 1 << 7,
    HasComponentOrigin = 1 << 8,
    ComposeTransform = 1 << 9, // default ON
    IsSmoothingAnimation = 1 << 10,
    JustAddedToHost = 1 << 11,
    // Forces a DrawableProxy into the draw order even when the layout's current
    // paint/clip state wouldn't require one, so the proxy can draw or (more
    // often) be sorted as a hit target. Set for any of: a clip a keyframe/data
    // bind can toggle on at runtime, a scroll/drag interaction target, or a
    // state-machine pointer listener target. See the mark* helpers below.
    ForceDrawableProxy = 1 << 12,
    // Transient per-draw flag: drawProxy() issued a renderer->save() for
    // clipping, so draw() owes the matching restore(). Guards against restoring
    // when drawProxy() never ran (e.g. a pure container whose proxy isn't in
    // the draw order but whose clip() turned on at runtime).
    ClipSaved = 1 << 13,
};

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

// The render-path buffers a LayoutComponent needs only when it actually paints,
// clips, has a foreground drawable, or is an Artboard. These are ~240 bytes
// together; heap-allocating them on first use keeps a plain container layout
// (which never paints) from carrying them inline. `background` is scratch for
// building the rounded-rect; `local`/`world` match the old member defaults
// (both default-constructed local, world switched at rewind time).
struct LayoutRenderPaths
{
    RawPath background;
    ShapePaintPath local;
    ShapePaintPath world;
};

class LayoutComponent : public LayoutComponentBase,
                        public ProxyDrawing,
                        public ShapePaintContainer,
                        public AdvancingComponent,
                        public InterpolatorHost,
                        public LayoutNodeProvider,
                        public LayoutStyleApplier
{
protected:
    LayoutComponentStyle* m_style = nullptr;
    LayoutData* m_layoutData;

    Layout m_layout;
    LayoutPadding m_layoutPadding;

    LayoutAnimationData m_animationDataA;
    LayoutAnimationData m_animationDataB;
    KeyFrameInterpolator* m_inheritedInterpolator;
    // The two 1-byte enums and the 2-byte flags word are kept adjacent so they
    // pack into a single 4-byte slot with no padding.
    LayoutStyleInterpolation m_inheritedInterpolation =
        LayoutStyleInterpolation::hold;
    LayoutDirection m_inheritedDirection = LayoutDirection::inherit;
    LayoutComponentFlags m_layoutFlags =
        LayoutComponentFlags::ParentIsRow |
        LayoutComponentFlags::PositionLeftChanged |
        LayoutComponentFlags::PositionTopChanged |
        LayoutComponentFlags::ComposeTransform;
    float m_inheritedInterpolationTime = 0;
    // Null until this layout first paints/clips/has a foreground drawable, or
    // (for Artboard) builds its background/clip path. See LayoutRenderPaths.
    std::unique_ptr<LayoutRenderPaths> m_renderPaths;
    // Lazily created: only layouts that actually paint, clip, or can gain a
    // clip at runtime (see needsDrawableProxy) — or that are referenced as a
    // scroll/listener target — ever allocate a proxy. A pure container layout
    // never pays for one. See DrawableProxy for why the object is needed at
    // all.
    std::unique_ptr<DrawableProxy> m_proxy;

    bool hasLayoutFlag(LayoutComponentFlags f) const
    {
        return (m_layoutFlags & f) != LayoutComponentFlags::None;
    }
    void setLayoutFlag(LayoutComponentFlags f, bool on)
    {
        if (on)
        {
            m_layoutFlags |= f;
        }
        else
        {
            m_layoutFlags &= ~f;
        }
    }

    // Allocates the render-path buffers on first use. Callers that build or
    // hand out a path go through this; the ShapePaintContainer accessors
    // (worldPath/localPath/localClockwisePath) instead return null while unset,
    // which is a valid contract and keeps plain containers allocation-free.
    LayoutRenderPaths& mutableRenderPaths()
    {
        if (m_renderPaths == nullptr)
        {
            m_renderPaths = std::make_unique<LayoutRenderPaths>();
        }
        return *m_renderPaths;
    }

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
    bool isCollapsed() const override;
    void propagateCollapse(bool collapse);
    bool collapse(bool value) override;
    float computedLocalX() override { return computedOriginLocal().x; };
    float computedLocalY() override { return computedOriginLocal().y; };
    float computedWorldX() override
    {
        return (worldTransform() * localAnchor()).x;
    };
    float computedWorldY() override
    {
        return (worldTransform() * localAnchor()).y;
    };
    float computedRootX() override;
    float computedRootY() override;
    float computedWidth() override { return m_layout.width(); };
    float computedHeight() override { return m_layout.height(); };
    void calculateLayoutInternal(float availableWidth, float availableHeight);

private:
    float m_widthOverride = NAN;
    float m_heightOverride = NAN;
    float m_forcedWidth = NAN;
    float m_forcedHeight = NAN;
    // Yoga YGUnit values (0..3) plus a -1 sentinel; int8_t is ample.
    int8_t m_widthUnitValueOverride = -1;
    int8_t m_heightUnitValueOverride = -1;
    // Remaining boolean state now lives in m_layoutFlags. Two flags carry the
    // notable defaults documented here:
    // - ComposeTransform (default ON): files exported before 7.3 never composed
    //   a layout's own rotation/scale, so any stored value was ignored. Import
    //   clears it for those files; the default keeps current behavior so a
    //   layout built outside of import isn't stuck on the legacy path. See
    //   File::minorVersion.

    /// Where the layout engine placed us, in the parent's transform frame.
    Vec2D layoutTranslation() const;
    /// The stored x/y offset plus rotation/scale about the pivot.
    Mat2D buildOwnTransform() const;
    /// The origin's position in the parent's frame.
    Vec2D computedOriginLocal() const
    {
        return layoutTranslation() + m_Transform * localAnchor();
    }

#ifdef WITH_RIVE_LAYOUT
protected:
    void propagateSizeToChildren(ContainerComponent* component);
    bool applyInterpolation(float elapsedSeconds, bool animate = true);
    bool styleDisplayHidden() const;
#endif

public:
    // Implemented for ShapePaintContainer.
    const Mat2D& shapeWorldTransform() const override
    {
        return worldTransform();
    }

    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;

    LayoutComponentStyle* style() { return m_style; }
    void style(LayoutComponentStyle* style) { m_style = style; }

    void draw(Renderer* renderer) override;
    void drawProxy(Renderer* renderer) override;
    bool isProxyHidden() override { return isHidden(); }
    Core* hitTest(HitInfo*, const Mat2D&) override;
    bool hitTestPoint(const Vec2D& position,
                      bool skipOnUnclipped,
                      bool isPrimaryHit) override;
    DrawableProxy* proxy()
    {
        if (m_proxy == nullptr)
        {
            m_proxy = std::make_unique<DrawableProxy>(this);
        }
        return m_proxy.get();
    };
    // True if this layout has (or can gain) something for its proxy to draw or
    // hit-test: a background/stroke, an active clip, or one of the deferred
    // reasons stamped as ForceDrawableProxy (runtime-toggleable clip, scroll
    // interaction target, or pointer-listener target). When true the artboard
    // injects the proxy into the draw order so it can be sorted for both
    // drawing and hit-testing.
    bool needsDrawableProxy() const
    {
        return clip() || !shapePaints().empty() ||
               hasLayoutFlag(LayoutComponentFlags::ForceDrawableProxy);
    }
    // The mark* helpers below are the distinct call sites that all set the one
    // ForceDrawableProxy bit; they differ only in why the proxy is needed.

    // Called when a keyframe or data bind that targets this layout's `clip`
    // resolves, guaranteeing a proxy before the artboard's first sort.
    void markClipMayBeDynamic()
    {
        setLayoutFlag(LayoutComponentFlags::ForceDrawableProxy, true);
    }
    // Called from a scroll/scrollbar constraint's buildDependencies() (which
    // runs before the artboard's one-time proxy injection, on every instance)
    // when this layout is its viewport/thumb/track. Without the proxy in the
    // draw order, the drag hit target can't be sorted and opaque content on top
    // would swallow the gesture before the drag ever starts.
    void markInteractionTarget()
    {
        setLayoutFlag(LayoutComponentFlags::ForceDrawableProxy, true);
    }
    // Called from StateMachineListener::onAddedClean (which runs on the source
    // artboard before its one-time proxy injection) when a pointer listener
    // targets this layout. Carried to instances by clone(), since the listener
    // hook only runs on the source. Without the proxy in the draw order the hit
    // target can't be sorted and overlapping opaque content would swallow it.
    void markListenerTarget()
    {
        setLayoutFlag(LayoutComponentFlags::ForceDrawableProxy, true);
    }
    virtual void updateRenderPath();
    void update(ComponentDirt value) override;
    void updateTransform() override;
    void composeWorldTransform() override;
    /// Only our stored x/y — where the layout put us is not something we
    /// offset by. Files too old to compose x/y keep reporting their solved
    /// position.
    Vec2D composedTranslation() const override
    {
        return composesLayoutOffset() ? Vec2D(x(), y()) : layoutTranslation();
    }
    /// Our box starts at local zero, so the origin is the anchor. Zero for
    /// artboards, which already draw about theirs. In the .cpp: is<Artboard>
    /// needs the complete type.
    Vec2D localAnchor() const override;
    bool composesLayoutOffset() const;
    void onDirty(ComponentDirt value) override;
    AABB layoutBounds() override
    {
        return AABB::fromLTWH(m_layout.left(),
                              m_layout.top(),
                              m_layout.width(),
                              m_layout.height());
    }
    size_t numLayoutNodes() override { return 1; }
    AABB constraintBounds() const override { return localBounds(); }
    AABB localBounds() const override
    {
        return AABB::fromLTWH(0.0f, 0.0f, m_layout.width(), m_layout.height());
    }
    virtual AABB worldBounds() const
    {
        auto transform = worldTransform();
        return AABB::fromLTWH(transform[4],
                              transform[5],
                              m_layout.width(),
                              m_layout.height());
    }

    float layoutX() const { return m_layout.left(); }
    float layoutY() const { return m_layout.top(); }
    /// The origin as a fraction of our size, from the optional
    /// ComponentOrigin child; 0 when absent. Named apart from Artboard's
    /// inline originX/originY so the two never shadow; Artboard overrides.
    virtual float pivotOriginX() const;
    virtual float pivotOriginY() const;
    void markHasComponentOrigin()
    {
        setLayoutFlag(LayoutComponentFlags::HasComponentOrigin, true);
    }
    Vec2D originOffset() const;
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

    float gapHorizontal();
    float gapVertical();

    // We provide a way for nested artboards (or other objects) to override this
    // layout's width/height and unit values.
    void widthOverride(float width, int unitValue = 1, bool isRow = true);
    void heightOverride(float height, int unitValue = 1, bool isRow = true);
    void parentIsRow(bool isRow);
    void parentIsStack(bool isStack);
    void widthIntrinsicallySizeOverride(bool intrinsic);
    void heightIntrinsicallySizeOverride(bool intrinsic);
    virtual bool canHaveOverrides() { return false; }
    // Honours the NestedArtboardLayout override; shared via LayoutSyncContext.
    bool effectiveParentIsRow();
#ifdef WITH_RIVE_LAYOUT
    // The scale type this layout is actually sized by: the host's override
    // when it has one (a hosted artboard's own style is not what the user set
    // on the NestedArtboardLayout), else its own style's.
    LayoutScaleType effectiveWidthScaleType();
    LayoutScaleType effectiveHeightScaleType();
    void applyBaseStyle(YGStyle& style,
                        const LayoutSyncContext& context) override;
    void applyContainerStyle(YGStyle& style,
                             const LayoutSyncContext& context) override;
#endif
    bool mainAxisIsRow();
    bool mainAxisIsColumn();
    // Whether this layout stacks its children, for hosts that push the
    // container's state into a hosted artboard.
    bool isStackContainer();
    bool overridesKeyedInterpolation(int propertyKey) override;
    bool hasShapePaints() const { return m_ShapePaints.size() > 0; }
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

    void* layoutNode(int index) override;
    void syncStyle();
    void syncLayoutChildren();
    void clearLayoutChildren();
    virtual void propagateSize();
    void updateLayoutBounds(bool animate = true) override;
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
                            LayoutDirection direction) override;
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
    void layoutTypeChanged();
    void directionChanged();
    LayoutDirection actualDirection();
    // Re-run each child provider's style sync: a participant's yoga style
    // (flexGrow vs alignSelf/justifySelf/grid placement) is computed against
    // our grid/flex type + main axis, so switching those must re-sync, not just
    // mark nodes dirty.
    void syncChildProviderStyles();
#endif

    void markPositionLeftChanged()
    {
        setLayoutFlag(LayoutComponentFlags::PositionLeftChanged, true);
    }
    void markPositionTopChanged()
    {
        setLayoutFlag(LayoutComponentFlags::PositionTopChanged, true);
    }
    void buildDependencies() override;

#ifdef WITH_RIVE_LAYOUT
    void addLayoutStyleApplier(LayoutStyleApplier* applier) override;
#endif
    void markLayoutNodeDirty(
        bool shouldForceUpdateLayoutBounds = false) override;
    void markLayoutStyleDirty();
    void clipChanged() override;
    void registerForegroundDrawable()
    {
        setLayoutFlag(LayoutComponentFlags::HasForegroundDrawable, true);
    }
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

// The two upward walks a component can ask about its layout, mirroring the two
// downward ones. contentSizingLayout mirrors propagateSizeToChildren: for
// content, and a group stops it. owningLayout mirrors forEachLayoutProvider:
// for anything in the layout tree, and nothing stops it.
//
// Both end at the artboard, which is a LayoutComponent whose own parent is
// null. So a component directly in the artboard resolves to it, and neither
// walk can escape an ArtboardInstance into the artboard hosting it.
LayoutComponent* contentSizingLayout(Component* parent);
LayoutComponent* owningLayout(Component* component);
} // namespace rive

#endif