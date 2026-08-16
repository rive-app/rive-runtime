#include "rive/layout/layout_participant.hpp"
#include "rive/layout_component.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout/layout_node_style.hpp"
#include "rive/layout/grid_item_placement.hpp"
#include "rive/layout/grid_track.hpp"
#include "rive/layout/layout_data.hpp"
#include "rive/layout/layout_style_applier.hpp"
#include "rive/constraints/layout_constraint.hpp"
#include "rive/transform_component.hpp"
#include "rive/intrinsically_sizeable.hpp"
#include "rive/component.hpp"
#include "rive/solo.hpp"
#include "rive/artboard.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include <algorithm>
#include <cmath>

using namespace rive;

#ifdef WITH_RIVE_LAYOUT
void LayoutParticipant::addLayoutStyleApplier(LayoutStyleApplier* applier)
{
    if (m_layoutData != nullptr)
    {
        m_layoutData->addApplier(applier);
    }
}
#endif

namespace rive
{
// Lazily-allocated animation state — only exists while a participant is under
// an animated layout, so a non-animating participant pays only the pointer.
struct ParticipantAnimation
{
    Layout animatedLayout;
    LayoutAnimationData a;
    LayoutAnimationData b;
    bool isSmoothing = false;
    LayoutStyleInterpolation interpolation = LayoutStyleInterpolation::hold;
    KeyFrameInterpolator* interpolator = nullptr;
    float interpolationTime = 0.0f;
};
} // namespace rive

#ifdef WITH_RIVE_LAYOUT
static YGSize participantMeasureFunc(YGNode* node,
                                     float width,
                                     YGMeasureMode widthMode,
                                     float height,
                                     YGMeasureMode heightMode)
{
    auto* component = static_cast<Component*>(node->getContext());
    auto* sizeable = IntrinsicallySizeable::from(component);
    Vec2D size = sizeable != nullptr
                     ? sizeable->measureLayout(width,
                                               (LayoutMeasureMode)widthMode,
                                               height,
                                               (LayoutMeasureMode)heightMode)
                     : Vec2D();
    return YGSize{size.x, size.y};
}
#endif

LayoutParticipant::~LayoutParticipant()
{
    delete m_animation;
#ifdef WITH_RIVE_LAYOUT
    releaseLayoutData();
#endif
}

TransformComponent* LayoutParticipant::transformComponent()
{
    auto* p = parent();
    return (p != nullptr && p->is<TransformComponent>())
               ? p->as<TransformComponent>()
               : nullptr;
}

void LayoutParticipant::applyLayoutConstraints()
{
    for (auto parentConstraint : layoutConstraints())
    {
        parentConstraint->constrainChild(this);
    }
}

LayoutComponent* LayoutParticipant::owningLayout()
{
    for (Component* c = parent(); c != nullptr; c = c->parent())
    {
        if (c->is<LayoutComponent>())
        {
            return c->as<LayoutComponent>();
        }
    }
    return nullptr;
}

bool LayoutParticipant::isParticipatingInLayout() const
{
#ifdef WITH_RIVE_LAYOUT
    return m_layoutData != nullptr;
#else
    return false;
#endif
}

StatusCode LayoutParticipant::onAddedClean(CoreContext* context)
{
    StatusCode code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
#ifdef WITH_RIVE_LAYOUT
    resync();
#endif
    return StatusCode::Ok;
}

#ifdef WITH_RIVE_LAYOUT
void LayoutParticipant::releaseLayoutData()
{
    if (m_layoutData == nullptr)
    {
        return;
    }
#ifdef WITH_RIVE_TOOLS
    m_layoutData->unref();
#else
    delete m_layoutData;
#endif
    m_layoutData = nullptr;
}

// Releases a fill axis from any content-based minimum, guarding against CSS's
// min-width:auto. Yoga has no such minimum today, so this is currently inert —
// see layout_fill_content_floor_test.dart, which fails if that changes.
// LayoutComponent has no equivalent.
//
// Overrides rather than sits inline so it lands after the min/max Super writes.
void LayoutParticipant::applyBaseStyle(YGStyle& style,
                                       const LayoutSyncContext& context)
{
    LayoutSizingStyle::applyBaseStyle(style, context);

    const LayoutScaleType widthScale = (LayoutScaleType)layoutWidthScaleType();
    const LayoutScaleType heightScale =
        (LayoutScaleType)layoutHeightScaleType();
    const bool parentIsRow = context.parentIsRow;
    const bool parentIsGridLike = context.parentIsGrid;
    const bool widthFill = widthScale == LayoutScaleType::fill;
    const bool heightFill = heightScale == LayoutScaleType::fill;

    style.dimensions()[YGDimensionWidth] =
        widthScale == LayoutScaleType::fixed
            ? YGValue{std::max(0.0f, width()), (YGUnit)widthUnitsValue()}
            : YGValue{YGUndefined, YGUnitAuto};
    style.dimensions()[YGDimensionHeight] =
        heightScale == LayoutScaleType::fixed
            ? YGValue{std::max(0.0f, height()), (YGUnit)heightUnitsValue()}
            : YGValue{YGUndefined, YGUnitAuto};

    if (parentIsGridLike)
    {
        style.flexGrow() = YGFloatOptional(0.0f);
        style.flexShrink() = YGFloatOptional(0.0f);
        style.alignSelf() = heightFill ? YGAlignStretch : YGAlignAuto;
    }
    else
    {
        bool mainFill = parentIsRow ? widthFill : heightFill;
        float mainFraction =
            parentIsRow ? fractionalWidth() : fractionalHeight();
        style.flexGrow() = YGFloatOptional(mainFill ? mainFraction : 0.0f);
        style.flexShrink() = YGFloatOptional(mainFill ? mainFraction : 0.0f);
        style.flexBasis() = mainFill ? YGValue{0.0f, YGUnitPoint}
                                     : YGValue{YGUndefined, YGUnitAuto};
        bool crossFill = parentIsRow ? heightFill : widthFill;
        style.alignSelf() = crossFill ? YGAlignStretch : YGAlignAuto;
    }

    if (layoutWidthScaleType() == (uint32_t)LayoutScaleType::fill &&
        (YGUnit)minWidthUnitsValue() == YGUnitUndefined)
    {
        style.minDimensions()[YGDimensionWidth] = YGValue{0.0f, YGUnitPoint};
    }
    if (layoutHeightScaleType() == (uint32_t)LayoutScaleType::fill &&
        (YGUnit)minHeightUnitsValue() == YGUnitUndefined)
    {
        style.minDimensions()[YGDimensionHeight] = YGValue{0.0f, YGUnitPoint};
    }
}

void LayoutParticipant::resync()
{
    auto* host = transformComponent();
    if (host == nullptr)
    {
        return;
    }
    if (m_layoutData == nullptr)
    {
        m_layoutData = new LayoutData();
        m_layoutData->node.getConfig()->setPointScaleFactor(0);
        // Measure our host's intrinsic (hug) size via IntrinsicallySizeable.
        m_layoutData->node.setContext(host);
        m_layoutData->node.setMeasureFunc(participantMeasureFunc);
        // We are our own sizing style.
        addLayoutStyleApplier(this);
    }
    // A sibling placement that cleaned before us was dropped on the floor:
    // addLayoutStyleApplier is a no-op until m_layoutData exists, and
    // onAddedClean runs in file order, so nothing fixes the order for us.
    // Idempotent — appliers are pushed unique.
    if (auto* placement = GridItemPlacement::from(host))
    {
        addLayoutStyleApplier(placement);
    }
    syncStyleChanges();
    if (auto* lc = owningLayout())
    {
        lc->syncLayoutChildren();
    }
    host->addDirt(ComponentDirt::WorldTransform, true);
    markLayoutNodeDirty(true);
}

void* LayoutParticipant::layoutNode(int index)
{
    return m_layoutData != nullptr ? static_cast<void*>(&m_layoutData->node)
                                   : nullptr;
}

Layout LayoutParticipant::solvedLayout() const
{
    if (m_layoutData == nullptr)
    {
        return Layout();
    }
    const auto& l = m_layoutData->node.getLayout();
    return Layout(definedOrZero(l.position[YGEdgeLeft]),
                  definedOrZero(l.position[YGEdgeTop]),
                  definedOrZero(l.dimensions[YGDimensionWidth]),
                  definedOrZero(l.dimensions[YGDimensionHeight]));
}

// While animating, the resolved slot is the interpolated animatedLayout;
// otherwise it's read straight from the yoga node (no per-participant cache).
float LayoutParticipant::resolvedLeft() const
{
    return m_animation != nullptr ? m_animation->animatedLayout.left()
                                  : solvedLayout().left();
}
float LayoutParticipant::resolvedTop() const
{
    return m_animation != nullptr ? m_animation->animatedLayout.top()
                                  : solvedLayout().top();
}
float LayoutParticipant::resolvedWidth() const
{
    return m_animation != nullptr ? m_animation->animatedLayout.width()
                                  : solvedLayout().width();
}
float LayoutParticipant::resolvedHeight() const
{
    return m_animation != nullptr ? m_animation->animatedLayout.height()
                                  : solvedLayout().height();
}

void LayoutParticipant::applyResolvedLayoutSize()
{
    auto* sizeable = IntrinsicallySizeable::from(transformComponent());
    if (sizeable == nullptr)
    {
        return;
    }
    auto* lc = owningLayout();
    LayoutDirection direction =
        lc != nullptr ? lc->actualDirection() : LayoutDirection::inherit;
    // Read the resolved slot once (animated slot, or a single yoga-node read)
    // rather than calling resolvedWidth()/resolvedHeight() separately.
    const Layout resolved =
        m_animation != nullptr ? m_animation->animatedLayout : solvedLayout();
    sizeable->controlSize(Vec2D(resolved.width(), resolved.height()),
                          (LayoutScaleType)layoutWidthScaleType(),
                          (LayoutScaleType)layoutHeightScaleType(),
                          direction);
}
#else
float LayoutParticipant::resolvedLeft() const { return 0.0f; }
float LayoutParticipant::resolvedTop() const { return 0.0f; }
float LayoutParticipant::resolvedWidth() const { return 0.0f; }
float LayoutParticipant::resolvedHeight() const { return 0.0f; }
#endif

size_t LayoutParticipant::numLayoutNodes()
{
#ifdef WITH_RIVE_LAYOUT
    return m_layoutData != nullptr ? 1 : 0;
#else
    return 0;
#endif
}

AABB LayoutParticipant::layoutBounds()
{
#ifdef WITH_RIVE_LAYOUT
    // Read the resolved slot once (animated slot, or a single yoga-node read)
    // instead of resolvedLeft/Top/Width/Height each re-reading it.
    const Layout resolved =
        m_animation != nullptr ? m_animation->animatedLayout : solvedLayout();
    return AABB::fromLTWH(resolved.left(),
                          resolved.top(),
                          resolved.width(),
                          resolved.height());
#else
    return AABB::fromLTWH(0.0f, 0.0f, 0.0f, 0.0f);
#endif
}

AABB LayoutParticipant::layoutBoundsForNode(int index)
{
#ifdef WITH_RIVE_LAYOUT
#endif
    return layoutBounds();
}

bool LayoutParticipant::syncStyleChanges()
{
#ifdef WITH_RIVE_LAYOUT
    if (m_layoutData == nullptr)
    {
        return false;
    }
    YGNode& node = m_layoutData->node;
    // Appliers write straight into the node's own style; no scratch copy.
    YGStyle& ygStyle = m_layoutData->style();

    LayoutScaleType widthScale = (LayoutScaleType)layoutWidthScaleType();
    LayoutScaleType heightScale = (LayoutScaleType)layoutHeightScaleType();

    auto* lc = owningLayout();
    bool parentIsRow = lc != nullptr ? lc->mainAxisIsRow() : true;
    bool parentIsGridLike =
        lc != nullptr && lc->style() != nullptr && lc->style()->isGrid();

    bool needsMeasure = widthScale == LayoutScaleType::hug ||
                        heightScale == LayoutScaleType::hug;
    if (needsMeasure)
    {
        node.setContext(transformComponent());
        node.setMeasureFunc(participantMeasureFunc);
    }
    else
    {
        node.setMeasureFunc(nullptr);
    }

    bool parentIsStack =
        lc != nullptr && lc->style() != nullptr && lc->style()->isStack();
    uint32_t containerJustifyItems = (lc != nullptr && lc->style() != nullptr)
                                         ? lc->style()->justifyItemsValue()
                                         : (uint32_t)YGJustifyStretch;
    // Appliers last, before the style reaches the node. A participant's
    // GridItemPlacement hangs off the same Node that owns this participant.
    LayoutSyncContext syncContext;
    syncContext.parentIsGrid = parentIsGridLike;
    syncContext.parentIsStack = parentIsStack;
    syncContext.containerJustifyItems = containerJustifyItems;
    syncContext.inlineHugs = widthScale == LayoutScaleType::hug;
    syncContext.parentIsRow = parentIsRow;
    syncContext.isLTR =
        lc == nullptr || lc->actualDirection() != LayoutDirection::rtl;
    syncContext.hasLayoutParent = lc != nullptr;
    m_layoutData->applyLayoutStyles(ygStyle, syncContext);

    node.markDirtyAndPropagate();
    // Fold display:none into the host's collapse so it stops drawing (it's
    // already removed from the layout flow via the yoga display above).
    if (auto* host = transformComponent())
    {
        auto* p = host->parent();
        bool parentHidesHost = p != nullptr && p->isCollapsed();
        // A Solo hides its non-active children; mirror that here so folding our
        // display doesn't reveal an inactive Solo child. (Dart re-dispatches
        // through the parent's collapse; C++ has no host mixin, so we check
        // it.)
        if (p != nullptr && p->is<Solo>())
        {
            auto* solo = p->as<Solo>();
            auto* ab = solo->artboard();
            Core* active = ab != nullptr
                               ? ab->resolve(solo->activeComponentId())
                               : nullptr;
            if (active != host)
            {
                parentHidesHost = true;
            }
        }
        host->collapse(parentHidesHost ||
                       (YGDisplay)displayValue() == YGDisplayNone);
    }
    return true;
#else
    return false;
#endif
}

void LayoutParticipant::updateLayoutBounds(bool animate)
{
#ifdef WITH_RIVE_LAYOUT
    if (m_layoutData == nullptr)
    {
        return;
    }
    YGNode& node = m_layoutData->node;
    if (!node.getHasNewLayout())
    {
        return;
    }
    node.setHasNewLayout(false);

    Layout newLayout = solvedLayout();
    // Animate only when under an animated layout (m_animation allocated), the
    // animate flag is set, and we've solved before (so we snap on first
    // appearance instead of animating in from 0,0).
    if (m_animation != nullptr && animate && m_hasSolvedLayout)
    {
        // Retarget: animate from where we are now to the newly solved slot,
        // smoothing over any in-flight animation (mirrors LayoutComponent).
        auto* animationData = currentAnimationData();
        if (newLayout != animationData->to)
        {
            if (animationData->elapsedSeconds != 0.0f)
            {
                if (m_animation->isSmoothing)
                {
                    m_animation->a.copy(m_animation->b);
                }
                m_animation->isSmoothing = true;
            }
            else
            {
                m_animation->isSmoothing = false;
            }
            animationData = currentAnimationData();
            animationData->from = m_animation->animatedLayout;
            animationData->to = newLayout;
            animationData->elapsedSeconds = 0.0f;
        }
    }
    else if (m_animation != nullptr)
    {
        // Snap the animated slot (first solve, or the animate flag is off).
        m_animation->animatedLayout = newLayout;
        m_animation->a.to = newLayout;
    }
    // else: not animating — resolvedLeft etc. read the yoga node directly.
    m_hasSolvedLayout = true;
    applyResolvedLayoutSize();
    if (auto* host = transformComponent())
    {
        host->addDirt(ComponentDirt::WorldTransform, true);
    }
#endif
}

void LayoutParticipant::markLayoutNodeDirty(bool shouldForceUpdateLayoutBounds)
{
#ifdef WITH_RIVE_LAYOUT
    if (m_layoutData != nullptr)
    {
        m_layoutData->node.markDirtyAndPropagate();
    }
    if (auto* lc = owningLayout())
    {
        lc->markLayoutNodeDirty(shouldForceUpdateLayoutBounds);
    }
#endif
}

void LayoutParticipant::onSizingChanged()
{
#ifdef WITH_RIVE_LAYOUT
    syncStyleChanges();
    markLayoutNodeDirty();
#endif
}

// ── Layout animation. The participant has no animation style of its own; it
// inherits the parent layout's (stored via cascadeLayoutStyle) and interpolates
// its resolved slot toward each newly solved layout, advanced each frame as an
// AdvancingComponent.

LayoutAnimationData* LayoutParticipant::currentAnimationData()
{
    // Only called while animating (m_animation != nullptr).
    return m_animation->isSmoothing ? &m_animation->b : &m_animation->a;
}

bool LayoutParticipant::animates() const { return m_animation != nullptr; }

LayoutStyleInterpolation LayoutParticipant::interpolation() const
{
    return m_animation != nullptr ? m_animation->interpolation
                                  : LayoutStyleInterpolation::hold;
}

float LayoutParticipant::interpolationTime() const
{
    return m_animation != nullptr ? m_animation->interpolationTime : 0.0f;
}

KeyFrameInterpolator* LayoutParticipant::interpolator() const
{
    return m_animation != nullptr ? m_animation->interpolator : nullptr;
}

bool LayoutParticipant::advanceComponent(float elapsedSeconds,
                                         AdvanceFlags flags)
{
#ifdef WITH_RIVE_LAYOUT
    if (m_animation == nullptr ||
        (flags & AdvanceFlags::NewFrame) != AdvanceFlags::NewFrame)
    {
        return false;
    }
    return applyInterpolation(elapsedSeconds,
                              (flags & AdvanceFlags::Animate) ==
                                      AdvanceFlags::Animate ||
                                  (flags & AdvanceFlags::AdvanceNested) ==
                                      AdvanceFlags::AdvanceNested);
#else
    return false;
#endif
}

#ifdef WITH_RIVE_LAYOUT
bool LayoutParticipant::cascadeLayoutStyle(
    LayoutStyleInterpolation inheritedInterpolation,
    KeyFrameInterpolator* inheritedInterpolator,
    float inheritedInterpolationTime,
    LayoutDirection direction)
{
    // A participant has no animation style of its own; it inherits the parent
    // layout's. Allocate the animation state only while it actually animates.
    bool willAnimate =
        inheritedInterpolation != LayoutStyleInterpolation::hold &&
        inheritedInterpolationTime > 0.0f;
    if (willAnimate)
    {
        if (m_animation == nullptr)
        {
            m_animation = new ParticipantAnimation();
            // Seed from the current resolved slot so enabling animation
            // mid-life doesn't animate in from 0.
            Layout current = solvedLayout();
            m_animation->animatedLayout = current;
            m_animation->a.from = current;
            m_animation->a.to = current;
        }
        m_animation->interpolation = inheritedInterpolation;
        m_animation->interpolator = inheritedInterpolator;
        m_animation->interpolationTime = inheritedInterpolationTime;
    }
    else if (m_animation != nullptr)
    {
        // Parent no longer animates: drop the state and snap from here on.
        delete m_animation;
        m_animation = nullptr;
    }
    return willAnimate;
}

bool LayoutParticipant::applyInterpolation(float elapsedSeconds, bool animate)
{
    if (m_animation == nullptr)
    {
        return false;
    }
    auto* animationData = currentAnimationData();
    if (!animate || animationData->to == m_animation->animatedLayout)
    {
        return false;
    }
    if (m_animation->isSmoothing)
    {
        float f =
            std::fmin(1.0f,
                      interpolationTime() > 0.0f
                          ? m_animation->a.elapsedSeconds / interpolationTime()
                          : 1.0f);
        if (interpolation() != LayoutStyleInterpolation::linear &&
            interpolator() != nullptr)
        {
            f = interpolator()->transform(f);
        }
        m_animation->b.from = m_animation->a.interpolate(f);
        if (f == 1.0f)
        {
            m_animation->a.copy(m_animation->b);
            m_animation->isSmoothing = false;
        }
        else
        {
            m_animation->a.elapsedSeconds += elapsedSeconds;
        }
    }

    animationData = currentAnimationData();
    if (animationData->elapsedSeconds >= interpolationTime())
    {
        m_animation->animatedLayout = animationData->to;
        if (m_animation->isSmoothing)
        {
            m_animation->isSmoothing = false;
            m_animation->a.copy(m_animation->b);
            m_animation->a.elapsedSeconds = m_animation->b.elapsedSeconds =
                0.0f;
        }
        else
        {
            m_animation->a.elapsedSeconds = 0.0f;
        }
        applyResolvedLayoutSize();
        if (auto* host = transformComponent())
        {
            host->addDirt(ComponentDirt::WorldTransform, true);
        }
        return false;
    }

    float f =
        std::fmin(1.0f,
                  interpolationTime() > 0.0f
                      ? animationData->elapsedSeconds / interpolationTime()
                      : 1.0f);
    if (interpolation() != LayoutStyleInterpolation::linear &&
        interpolator() != nullptr)
    {
        f = interpolator()->transform(f);
    }
    auto current = animationData->interpolate(f);
    if (m_animation->animatedLayout != current)
    {
        m_animation->animatedLayout = current;
        applyResolvedLayoutSize();
        if (auto* host = transformComponent())
        {
            host->addDirt(ComponentDirt::WorldTransform, true);
        }
    }
    animationData->elapsedSeconds += elapsedSeconds;
    return f != 1.0f;
}
#endif
