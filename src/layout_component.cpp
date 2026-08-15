#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/constraints/layout_constraint.hpp"
#include "rive/drawable.hpp"
#include "rive/factory.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/intrinsically_sizeable.hpp"
#include "rive/layout_component.hpp"
#include "rive/component_origin.hpp"
#include "rive/layout/grid_track.hpp"
#include "rive/layout/layout_style_applier.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/node.hpp"
#include "rive/math/aabb.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/solo.hpp"
#include "rive/layout/layout_data.hpp"
#include "rive/layout/layout_component_style.hpp"
#ifdef WITH_RIVE_LAYOUT
#include "rive/transform_component.hpp"
#include "yoga/YGEnums.h"
#include "yoga/YGFloatOptional.h"
#endif
#include <vector>

using namespace rive;

#ifdef WITH_RIVE_LAYOUT
void LayoutComponent::addLayoutStyleApplier(LayoutStyleApplier* applier)
{
    if (m_layoutData != nullptr)
    {
        m_layoutData->addApplier(applier);
    }
}
#endif

namespace
{
// Containers transparent to layout provide no layout node and no
// sizing of their own, so the layout above descends through them. A plain group
// exposes all its children; a Solo exposes only its active one. Anything else
// (n-slicers, nested artboards, shapes) is opaque — a participant inside one
// would never be collected.
bool isTransparentLayoutContainer(Component* component)
{
    return component->coreType() == NodeBase::typeKey || component->is<Solo>();
}

// An ArtboardComponentList provides a layout node unconditionally — it never
// opted in the way a participant does. Wrapping one in a group is how a file
// says "render these items but don't lay them out", freeing them to be placed
// by x/y or a follow-path constraint, so a transparent container stays opaque
// to it unless the list opts in. Everything else is seen through.
bool joinsLayoutThroughContainer(Component* component)
{
    if (!component->is<ArtboardComponentList>())
    {
        return true;
    }
    auto flags =
        static_cast<DrawableFlag>(component->as<Drawable>()->drawableFlags());
    return (flags & DrawableFlag::ParticipatesInLayout) ==
           DrawableFlag::ParticipatesInLayout;
}

// Visit every provider that belongs to a layout — its direct children, plus any
// nested inside transparent containers; depth-first, so layout order follows
// hierarchy order. Nested LayoutComponents and providers are leaves here (they
// own their subtree). Every child-walking layout pass goes through this, so a
// nested participant is collected, sized, cascaded and re-synced like a direct
// child.
template <typename F>
void forEachLayoutProvider(Component* from, F&& visit, bool nested = false)
{
    // A Solo only lets its active child through.
    if (from->is<Solo>())
    {
        if (auto* active = from->as<Solo>()->activeComponent())
        {
            if (auto* provider = LayoutNodeProvider::from(active))
            {
                if (!nested || joinsLayoutThroughContainer(active))
                {
                    visit(active, provider);
                }
            }
            else if (isTransparentLayoutContainer(active))
            {
                forEachLayoutProvider(active, visit, true);
            }
        }
        return;
    }
    for (auto* child : from->as<ContainerComponent>()->children())
    {
        if (auto* provider = LayoutNodeProvider::from(child))
        {
            if (!nested || joinsLayoutThroughContainer(child))
            {
                visit(child, provider);
            }
        }
        else if (isTransparentLayoutContainer(child))
        {
            forEachLayoutProvider(child, visit, true);
        }
    }
}

// Only reached when m_hasComponentOrigin says one exists, so the common case
// never walks.
ComponentOrigin* originChild(const ContainerComponent* owner)
{
    for (auto* child : owner->children())
    {
        if (child->is<ComponentOrigin>())
        {
            return child->as<ComponentOrigin>();
        }
    }
    return nullptr;
}
} // namespace

#if defined(WITH_RIVE_LAYOUT) && defined(WITH_RIVE_TOOLS) && defined(DEBUG)
uint32_t LayoutData::count = 0;
#endif

float LayoutComponent::pivotOriginX() const
{
    if (!m_hasComponentOrigin)
    {
        return 0.0f;
    }
    auto* origin = originChild(this);
    return origin != nullptr ? origin->originX() : 0.0f;
}

float LayoutComponent::pivotOriginY() const
{
    if (!m_hasComponentOrigin)
    {
        return 0.0f;
    }
    auto* origin = originChild(this);
    return origin != nullptr ? origin->originY() : 0.0f;
}

void LayoutComponent::buildDependencies()
{
    Super::buildDependencies();
    if (parent() != nullptr)
    {
        parent()->addDependent(this);
    }
    // Set the blend mode on all the shape paints. If we ever animate this
    // property, we'll need to update it in the update cycle/mark dirty when the
    // blend mode changes.
    for (auto paint : m_ShapePaints)
    {
        paint->blendMode(blendMode());
    }
}

Core* LayoutComponent::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

bool LayoutComponent::hitTestPoint(const Vec2D& position,
                                   bool skipOnUnclipped,
                                   bool isPrimaryHit)
{
    Mat2D inverseWorld;
    if (worldTransform().invert(&inverseWorld))
    {
        // If the layout is not clipped and skipOnUnclipped is true, we
        // don't care about whether it contains the position
        auto canSkip = skipOnUnclipped && !this->clip();
        if (!canSkip)
        {
            auto localWorld = inverseWorld * position;
            if (this->is<Artboard>())
            {
                auto artboard = this->as<Artboard>();
                if (artboard->originX() != 0 || artboard->originY() != 0)
                {
                    localWorld +=
                        Vec2D(artboard->originX() * artboard->layoutWidth(),
                              artboard->originY() * artboard->layoutHeight());
                }
            }
            if (!localBounds().contains(localWorld))
            {
                return false;
            }
        }
        return Drawable::hitTestPoint(position, true, isPrimaryHit);
    }
    return false;
}

StatusCode LayoutComponent::import(ImportStack& importStack)
{
    // Files exported before 7.3 composed a layout's transform from the solved
    // slot alone, so any stored rotation/scale was written but never applied.
    // Keep that legacy behavior for those files; newer files compose it on top
    // of the slot. See File::minorVersion.
    int major = importStack.majorVersion();
    int minor = importStack.minorVersion();
    m_composeTransform = major > 7 || (major == 7 && minor >= 3);
    return Super::import(importStack);
}

Core* LayoutComponent::clone() const
{
    LayoutComponent* twin = LayoutComponentBase::clone()->as<LayoutComponent>();
    twin->m_composeTransform = m_composeTransform;
    return twin;
}

bool LayoutComponent::composesLayoutOffset() const
{
    return m_composeTransform && !is<Artboard>();
}

Vec2D LayoutComponent::localAnchor() const
{
    if (!m_hasComponentOrigin || is<Artboard>())
    {
        return Vec2D();
    }
    return originOffset();
}

Vec2D LayoutComponent::originOffset() const
{
    return Vec2D(pivotOriginX() * m_layout.width(),
                 pivotOriginY() * m_layout.height());
}

Vec2D LayoutComponent::layoutTranslation() const
{
    // Our own origin deliberately does not enter here: contents sit within
    // our box whatever it is, so nothing inside has to compensate. An
    // artboard's origin does define where its local zero sits, so step back
    // by it to align to its box.
    auto location = Vec2D(m_layout.left(), m_layout.top());
    if (parent() != nullptr && parent()->is<Artboard>())
    {
        auto art = parent()->as<Artboard>();
        location -= Vec2D(art->layoutWidth() * art->originX(),
                          art->layoutHeight() * art->originY());
    }
    return location;
}

Mat2D LayoutComponent::buildOwnTransform() const
{
    // Outermost, matching TransformComponent's T * R * S, so the offset is
    // not rotated or scaled by our own transform.
    Mat2D own =
        composesLayoutOffset()
            ? Mat2D::fromTranslation(Vec2D(NodeBase::x(), NodeBase::y()))
            : Mat2D();

    // Pivot about the origin. The box stays put, so wrap rather than fold
    // into the frame.
    if (m_composeTransform &&
        (rotation() != 0.0f || scaleX() != 1.0f || scaleY() != 1.0f))
    {
        Mat2D local =
            rotation() != 0.0f ? Mat2D::fromRotation(rotation()) : Mat2D();
        local.scaleByValues(scaleX(), scaleY());
        auto pivot = originOffset();
        if (pivot.x != 0.0f || pivot.y != 0.0f)
        {
            local = Mat2D::multiply(
                Mat2D::fromTranslate(pivot.x, pivot.y),
                Mat2D::multiply(local,
                                Mat2D::fromTranslate(-pivot.x, -pivot.y)));
        }
        own = Mat2D::multiply(own, local);
    }
    return own;
}

float LayoutComponent::computedRootX()
{
    return artboard()->rootTransform(worldTransform() * localAnchor()).x;
}

float LayoutComponent::computedRootY()
{
    return artboard()->rootTransform(worldTransform() * localAnchor()).y;
}

void LayoutComponent::updateTransform() { m_Transform = buildOwnTransform(); }

void LayoutComponent::composeWorldTransform()
{
    Mat2D parentWorld =
        parent() != nullptr && parent()->is<WorldTransformComponent>()
            ? parent()->as<WorldTransformComponent>()->worldTransform()
            : Mat2D();
    // Insert where the layout placed us, the same shape Shape/Text/Image
    // use for a participant.
    Mat2D base = Mat2D::fromTranslation(layoutTranslation());
    m_WorldTransform =
        Mat2D::multiply(Mat2D::multiply(parentWorld, base), m_Transform);
}

void LayoutComponent::update(ComponentDirt value)
{
    Super::update(value);
#ifdef WITH_RIVE_LAYOUT
    if (value == ComponentDirt::Filthy)
    {
        // Use this to prevent layout animation on startup
        interruptAnimation();
    }
#endif
    if (hasDirt(value, ComponentDirt::RenderOpacity))
    {
        propagateOpacity(childOpacity());
    }
    if (parent() != nullptr && hasDirt(value, ComponentDirt::WorldTransform))
    {
        // Not left to Super's Transform-dirt pass: the pivot scales by the
        // solved size, so a re-solve alone can stale it.
        m_Transform = buildOwnTransform();
        composeWorldTransform();
        updateConstraints();
    }
    if (hasDirt(value,
                ComponentDirt::Path | ComponentDirt::WorldTransform |
                    ComponentDirt::LayoutStyle))
    {
        updateRenderPath();
    }

    m_positionLeftChanged = false;
    m_positionTopChanged = false;
}

void LayoutComponent::widthOverride(float width, int unitValue, bool isRow)
{
    m_widthOverride = width;
    m_widthUnitValueOverride = unitValue;
    m_parentIsRow = isRow;
    markLayoutNodeDirty();
}

void LayoutComponent::heightOverride(float height, int unitValue, bool isRow)
{
    m_heightOverride = height;
    m_heightUnitValueOverride = unitValue;
    m_parentIsRow = isRow;
    markLayoutNodeDirty();
}

void LayoutComponent::parentIsRow(bool isRow)
{
    m_parentIsRow = isRow;
    markLayoutNodeDirty();
}

void LayoutComponent::parentIsStack(bool isStack)
{
    if (m_parentIsStack == isStack)
    {
        return;
    }
    m_parentIsStack = isStack;
    markLayoutNodeDirty();
}

void LayoutComponent::widthIntrinsicallySizeOverride(bool intrinsic)
{
    m_widthIntrinsicallySizeOverride = intrinsic;
    // If we have an intrinsically sized override, set units to auto
    // otherwise set to points
    m_widthUnitValueOverride = intrinsic ? 3 : 1;
    markLayoutNodeDirty();
}

void LayoutComponent::heightIntrinsicallySizeOverride(bool intrinsic)
{
    m_heightIntrinsicallySizeOverride = intrinsic;
    // If we have an intrinsically sized override, set units to auto
    // otherwise set to points
    m_heightUnitValueOverride = intrinsic ? 3 : 1;
    markLayoutNodeDirty();
}

void LayoutComponent::forcedWidth(float width)
{
    if (m_forcedWidth == width)
    {
        return;
    }
    m_forcedWidth = width;
    markLayoutStyleDirty();
    markLayoutNodeDirty();
}

void LayoutComponent::forcedHeight(float height)
{
    if (m_forcedHeight == height)
    {
        return;
    }
    m_forcedHeight = height;
    markLayoutStyleDirty();
    markLayoutNodeDirty();
}

void LayoutComponent::updateConstraints()
{
    if (layoutConstraints().size() > 0)
    {
        for (auto parentConstraint : layoutConstraints())
        {
            parentConstraint->constrainChild(this);
        }
    }
    Super::updateConstraints();
}

bool LayoutComponent::overridesKeyedInterpolation(int propertyKey)
{
#ifdef WITH_RIVE_LAYOUT
    if (animates())
    {
        switch (propertyKey)
        {
            case LayoutComponentBase::widthPropertyKey:
            case LayoutComponentBase::heightPropertyKey:
                return true;
            default:
                return false;
        }
    }
#endif
    return false;
}

bool LayoutComponent::isHidden() const
{
    return Super::isHidden() || isCollapsed();
}

bool LayoutComponent::isCollapsed() const
{
    if (Super::isCollapsed())
    {
        return true;
    }
#ifdef WITH_RIVE_LAYOUT
    return styleDisplayHidden();
#endif
    return false;
}

void LayoutComponent::propagateCollapse(bool collapse)
{
    auto collapsed = collapse || isCollapsed();
    for (Component* child : children())
    {
        child->collapse(collapsed);
    }
    updateCollapsables();
}

bool LayoutComponent::collapse(bool value)
{
    if (!Component::collapse(value))
    {
        return false;
    }
    propagateCollapse(value);
    return true;
}

#ifdef WITH_RIVE_LAYOUT

LayoutComponent::LayoutComponent() :
    m_layoutData(new LayoutData()), m_proxy(this)
{
    m_layoutData->node.getConfig()->setPointScaleFactor(0);
}

float LayoutComponent::gapHorizontal()
{
    if (m_style == nullptr)
    {
        return 0;
    }
    return m_style->gapHorizontalUnits() == YGUnitPercent
               ? (m_style->gapHorizontal() / 100.0f) * layoutWidth()
               : m_style->gapHorizontal();
}

float LayoutComponent::gapVertical()
{
    if (m_style == nullptr)
    {
        return 0;
    }
    return m_style->gapVerticalUnits() == YGUnitPercent
               ? (m_style->gapVertical() / 100.0f) * layoutHeight()
               : m_style->gapVertical();
}

StatusCode LayoutComponent::onAddedDirty(CoreContext* context)
{
    auto code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    auto coreStyle = context->resolve(styleId());
    if (coreStyle == nullptr || !coreStyle->is<LayoutComponentStyle>())
    {
        return StatusCode::MissingObject;
    }
    m_style = static_cast<LayoutComponentStyle*>(coreStyle);
    addChild(m_style);
#ifdef WITH_RIVE_LAYOUT
    // We contribute sizing the style cannot see; it contributes the rest.
    addLayoutStyleApplier(this);
    addLayoutStyleApplier(m_style);
#endif

    return StatusCode::Ok;
}

StatusCode LayoutComponent::onAddedClean(CoreContext* context)
{
    auto code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    markLayoutStyleDirty();
    syncLayoutChildren();
    propagateCollapse(isCollapsed());
    return StatusCode::Ok;
}

void LayoutComponent::drawProxy(Renderer* renderer)
{
    if (clip())
    {
        renderer->save();
        renderer->clipPath(m_worldPath.renderPath(this));
    }
    for (auto shapePaint : m_ShapePaints)
    {
        if (!shapePaint->shouldDraw())
        {
            continue;
        }
        auto shapePaintPath = shapePaint->pickPath(this);
        if (shapePaintPath == nullptr)
        {
            continue;
        }
        shapePaint->draw(renderer, shapePaintPath, worldTransform());
    }
}

void LayoutComponent::draw(Renderer* renderer)
{
    // Restore clip before drawing stroke so we don't clip the stroke
    if (clip())
    {
        renderer->restore();
    }
}

void LayoutComponent::updateRenderPath()
{
    if (isHidden())
    {
        return;
    }
    if (m_ShapePaints.empty() && !clip() && !m_hasForegroundDrawable)
    {
        return;
    }
    float tl = 0.0f, tr = 0.0f, br = 0.0f, bl = 0.0f;
    if (style() != nullptr)
    {
        const bool isLTR = actualDirection() != LayoutDirection::rtl;
        if (style()->linkCornerRadius())
        {
            tl = tr = br = bl = style()->cornerRadiusTL();
        }
        else
        {
            tl = isLTR ? style()->cornerRadiusTL() : style()->cornerRadiusTR();
            tr = isLTR ? style()->cornerRadiusTR() : style()->cornerRadiusTL();
            bl = isLTR ? style()->cornerRadiusBL() : style()->cornerRadiusBR();
            br = isLTR ? style()->cornerRadiusBR() : style()->cornerRadiusBL();
        }
    }

    m_backgroundRawPath.rewind();
    Path::addRoundedRect(
        m_backgroundRawPath,
        AABB::fromLTWH(0.0f, 0.0f, m_layout.width(), m_layout.height()),
        tl,
        tr,
        br,
        bl);

    m_localPath.rewind();
    m_localPath.addPath(m_backgroundRawPath);

    m_worldPath.rewind(false, FillRule::clockwise);
    m_worldPath.addPath(m_backgroundRawPath, &m_WorldTransform);

    for (auto shapePaint : m_ShapePaints)
    {
        if (!shapePaint->shouldDraw())
        {
            continue;
        }
        if (shapePaint->is<ShapePaint>())
        {
            shapePaint->as<ShapePaint>()->invalidateEffects();
        }
    }
}

static YGSize measureFunc(YGNode* node,
                          float width,
                          YGMeasureMode widthMode,
                          float height,
                          YGMeasureMode heightMode)
{
    Vec2D size = ((LayoutComponent*)node->getContext())
                     ->measureLayout(width,
                                     (LayoutMeasureMode)widthMode,
                                     height,
                                     (LayoutMeasureMode)heightMode);

    return YGSize{size.x, size.y};
}

Vec2D LayoutComponent::measureLayout(float width,
                                     LayoutMeasureMode widthMode,
                                     float height,
                                     LayoutMeasureMode heightMode)
{
    Vec2D size = Vec2D();
    for (auto child : children())
    {
        if (child->is<LayoutComponent>())
        {
            continue;
        }
        auto sizeableChild = IntrinsicallySizeable::from(child);
        if (sizeableChild != nullptr)
        {
            Vec2D measured = sizeableChild->measureLayout(width,
                                                          widthMode,
                                                          height,
                                                          heightMode);
            size = Vec2D(std::max(size.x, measured.x),
                         std::max(size.y, measured.y));
        }
    }
    return size;
}

bool LayoutComponent::effectiveParentIsRow()
{
    if (canHaveOverrides())
    {
        return m_parentIsRow;
    }
    return layoutParent() != nullptr ? layoutParent()->mainAxisIsRow() : true;
}

bool LayoutComponent::mainAxisIsRow()
{
    if (style() == nullptr)
    {
        return true;
    }
    return style()->flexDirection() == YGFlexDirectionRow ||
           style()->flexDirection() == YGFlexDirectionRowReverse;
}

bool LayoutComponent::mainAxisIsColumn()
{
    if (style() == nullptr)
    {
        return false;
    }
    return style()->flexDirection() == YGFlexDirectionColumn ||
           style()->flexDirection() == YGFlexDirectionColumnReverse;
}

bool LayoutComponent::isStackContainer()
{
    return style() != nullptr && style()->isStack();
}

bool LayoutComponent::isLeaf()
{
    bool leaf = true;
    forEachLayoutProvider(this, [&leaf](Component*, LayoutNodeProvider*) {
        leaf = false;
    });
    return leaf;
}

void* LayoutComponent::layoutNode(int index)
{
    if (m_layoutData != nullptr)
    {
        return static_cast<void*>(&m_layoutData->node);
    }
    return nullptr;
}

#ifdef WITH_RIVE_LAYOUT
// Tracks come from GridTrack children, which the style cannot walk. A stack has
// no authored tracks — the style sets up its implicit cell instead.
void LayoutComponent::applyContainerStyle(YGStyle& ygStyle,
                                          const LayoutSyncContext& context)
{
    if (m_style == nullptr || m_style->isStack())
    {
        return;
    }
    GridTrack::syncContainerStyle(ygStyle, this, m_style->justifyItemsValue());
}
#endif

#ifdef WITH_RIVE_LAYOUT
// This layout's own size and how it sits in its parent's flow. On the
// component, not the style, because it reads width()/height(), the
// NestedArtboardLayout overrides and the forced dimensions.
//
// Not shared with LayoutParticipant::applyBaseStyle: the participant has no
// flexBasis, no overrides and its own fractionalWidth, so merging would change
// behaviour.
void LayoutComponent::applyBaseStyle(YGStyle& ygStyle,
                                     const LayoutSyncContext& context)
{
    if (m_style == nullptr)
    {
        return;
    }
    const bool parentIsRow = context.parentIsRow;
    const bool parentIsGridLike = context.parentIsGrid;

    // Derive the unit the layout engine needs from the persisted unit + scale
    // type.
    //   - fill/hug always map to YGUnitAuto (regardless of position type) —
    //     these scale modes don't have a meaningful unit; the layout engine
    //     decides the size.
    //   - fixed uses the stored unit. Absolute layouts trust whatever's
    //     stored (e.g. the editor freezes a point value at the moment
    //     positionType becomes absolute, since absolute children sit outside
    //     flex flow).
    bool isAbsolute = m_style->positionType() == YGPositionTypeAbsolute;
    bool isLegacyHugEncoding =
        m_style->widthScaleType() == LayoutScaleType::fixed &&
        m_style->heightScaleType() == LayoutScaleType::fixed &&
        m_style->intrinsicallySized() && isLeaf();
    auto effectiveUnits = [isAbsolute,
                           isLegacyHugEncoding](LayoutScaleType scaleType,
                                                YGUnit storedUnits) {
        if (isAbsolute && scaleType != LayoutScaleType::hug)
        {
            return storedUnits;
        }
        if (scaleType != LayoutScaleType::fixed)
        {
            return YGUnitAuto;
        }
        if (storedUnits == YGUnitPoint || storedUnits == YGUnitPercent)
        {
            return storedUnits;
        }
        return isLegacyHugEncoding ? YGUnitAuto : YGUnitPoint;
    };
    auto realWidth = width();
    auto realWidthScaleType = m_style->widthScaleType();
    auto realWidthUnits =
        effectiveUnits(realWidthScaleType, m_style->widthUnits());
    auto realHeight = height();
    auto realHeightScaleType = m_style->heightScaleType();
    auto realHeightUnits =
        effectiveUnits(realHeightScaleType, m_style->heightUnits());

    // If we have override width/height values, use those.
    // Currently we only use these for Artboards that are part of a
    // NestedArtboardLayout but perhaps there will be other use cases for
    // overriding in the future?
    if (canHaveOverrides())
    {
        if (!std::isnan(m_widthOverride))
        {
            realWidth = m_widthOverride;
        }
        if (!std::isnan(m_heightOverride))
        {
            realHeight = m_heightOverride;
        }
        if (m_widthUnitValueOverride != -1)
        {
            realWidthUnits = YGUnit(m_widthUnitValueOverride);
            switch (realWidthUnits)
            {
                case YGUnitPoint:
                case YGUnitPercent:
                    realWidthScaleType = LayoutScaleType::fixed;
                    break;
                case YGUnitAuto:
                    if (m_widthIntrinsicallySizeOverride)
                    {
                        realWidthScaleType = LayoutScaleType::hug;
                    }
                    else
                    {
                        realWidthScaleType = LayoutScaleType::fill;
                    }
                    break;
                default:
                    break;
            }
        }
        if (m_heightUnitValueOverride != -1)
        {
            realHeightUnits = YGUnit(m_heightUnitValueOverride);
            switch (realHeightUnits)
            {
                case YGUnitPoint:
                case YGUnitPercent:
                    realHeightScaleType = LayoutScaleType::fixed;
                    break;
                case YGUnitAuto:
                    if (m_heightIntrinsicallySizeOverride)
                    {
                        realHeightScaleType = LayoutScaleType::hug;
                    }
                    else
                    {
                        realHeightScaleType = LayoutScaleType::fill;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    if (!std::isnan(m_forcedWidth))
    {
        ygStyle.dimensions()[YGDimensionWidth] =
            YGValue{std::max(0.0f, m_forcedWidth), YGUnitPoint};
    }
    else
    {
        ygStyle.dimensions()[YGDimensionWidth] =
            YGValue{std::max(0.0f, realWidth), realWidthUnits};
    }
    if (!std::isnan(m_forcedHeight))
    {
        ygStyle.dimensions()[YGDimensionHeight] =
            YGValue{std::max(0.0f, m_forcedHeight), YGUnitPoint};
    }
    else
    {
        ygStyle.dimensions()[YGDimensionHeight] =
            YGValue{std::max(0.0f, realHeight), realHeightUnits};
    }

    if (parentIsGridLike)
    {
        // Grid/stack: size per-axis. fill stretches that axis (width ->
        // justifySelf after grid placement below, height -> alignSelf);
        // non-fill stays auto so the container alignment positions us. flexGrow
        // ignored.
        ygStyle.flexGrow() = YGFloatOptional(0);
        ygStyle.flexShrink() = YGFloatOptional(0);
        ygStyle.alignSelf() = realHeightScaleType == LayoutScaleType::fill
                                  ? YGAlignStretch
                                  : YGAlignAuto;
    }
    else
    {
        switch (realWidthScaleType)
        {
            case LayoutScaleType::fixed:
                if (parentIsRow)
                {
                    ygStyle.flexGrow() = YGFloatOptional(0);
                    ygStyle.flexShrink() = YGFloatOptional(0);
                    ygStyle.flexBasis() =
                        YGValue{m_style->flexBasis(), YGUnitAuto};
                }
                else
                {
                    ygStyle.alignSelf() = YGAlignAuto;
                }
                break;
            case LayoutScaleType::fill:
                if (parentIsRow)
                {
                    ygStyle.flexGrow() = YGFloatOptional(fractionalWidth());
                    ygStyle.flexShrink() = YGFloatOptional(fractionalWidth());
                    ygStyle.flexBasis() = YGValue{m_style->flexBasis(),
                                                  m_style->flexBasisUnits()};
                }
                else
                {
                    ygStyle.alignSelf() = YGAlignStretch;
                }
                break;
            case LayoutScaleType::hug:
                if (parentIsRow)
                {
                    ygStyle.flexGrow() = YGFloatOptional(0);
                    ygStyle.flexShrink() = YGFloatOptional(0);
                    ygStyle.flexBasis() =
                        YGValue{m_style->flexBasis(), YGUnitAuto};
                }
                else
                {
                    ygStyle.alignSelf() = YGAlignAuto;
                }
                break;
            default:
                break;
        }

        switch (realHeightScaleType)
        {
            case LayoutScaleType::fixed:
                if (!parentIsRow)
                {
                    ygStyle.flexGrow() = YGFloatOptional(0);
                    ygStyle.flexShrink() = YGFloatOptional(0);
                    ygStyle.flexBasis() =
                        YGValue{m_style->flexBasis(), YGUnitAuto};
                }
                else
                {
                    ygStyle.alignSelf() = YGAlignAuto;
                }
                break;
            case LayoutScaleType::fill:
                if (!parentIsRow)
                {
                    ygStyle.flexGrow() = YGFloatOptional(fractionalHeight());
                    ygStyle.flexShrink() = YGFloatOptional(fractionalHeight());
                    ygStyle.flexBasis() = YGValue{m_style->flexBasis(),
                                                  m_style->flexBasisUnits()};
                }
                else
                {
                    ygStyle.alignSelf() = YGAlignStretch;
                }
                break;
            case LayoutScaleType::hug:
                if (!parentIsRow)
                {
                    ygStyle.flexGrow() = YGFloatOptional(0);
                    ygStyle.flexShrink() = YGFloatOptional(0);
                    ygStyle.flexBasis() =
                        YGValue{m_style->flexBasis(), YGUnitAuto};
                }
                else
                {
                    ygStyle.alignSelf() = YGAlignAuto;
                }
                break;
            default:
                break;
        }
    }
}
#endif

void LayoutComponent::syncStyle()
{
    if (m_style == nullptr || m_layoutData == nullptr)
    {
        return;
    }
    YGNode& ygNode = m_layoutData->node;
    // Appliers write straight into the node's own style; no scratch copy.
    YGStyle& ygStyle = m_layoutData->style();
    if (m_style->intrinsicallySized() && isLeaf())
    {
        ygNode.setContext(this);
        ygNode.setMeasureFunc(measureFunc);
    }
    else
    {
        ygNode.setMeasureFunc(nullptr);
    }

    // Our owning layout may sit above a transparent container (group/Solo), so
    // walk up for it rather than reading the immediate parent — a grouped item
    // is still laid out by the container and needs its context. Cached because
    // layoutParent() walks the chain on each call.
    auto* parentLayout = layoutParent();
    auto* parentLayoutStyle =
        parentLayout != nullptr ? parentLayout->style() : nullptr;
    // A hosted artboard (list item, nested artboard layout) has no parent() to
    // walk, so its host pushes the container's stack state in. Only stack is
    // pushed: a grid already auto-places these nodes correctly, and claiming
    // grid here would move them off the flex sizing path they use today.
    bool parentIsStack = parentLayoutStyle != nullptr
                             ? parentLayoutStyle->isStack()
                             : (canHaveOverrides() && m_parentIsStack);
    bool parentIsGridLike = parentLayoutStyle != nullptr
                                ? parentLayoutStyle->isGrid()
                                : parentIsStack;
    uint32_t containerJustifyItems =
        parentLayoutStyle != nullptr ? parentLayoutStyle->justifyItemsValue()
                                     : (uint32_t)YGJustifyStretch;
    // Every style write is an applier now; resolve the context and run them.
    LayoutSyncContext syncContext;
    syncContext.parentIsGrid = parentIsGridLike;
    syncContext.parentIsStack = parentIsStack;
    syncContext.containerJustifyItems = containerJustifyItems;
    syncContext.inlineHugs = m_style->widthScaleType() == LayoutScaleType::hug;
    syncContext.parentIsRow = effectiveParentIsRow();
    syncContext.isLTR = actualDirection() != LayoutDirection::rtl;
    syncContext.hasLayoutParent = layoutParent() != nullptr;
    m_layoutData->applyLayoutStyles(ygStyle, syncContext);

    // Sync the styles of participant children that provide their
    // own layout node (including any nested inside transparent groups).
    // LayoutComponent children sync their own styles through the dirty-layout
    // set; these have no such entry.
    forEachLayoutProvider(this,
                          [](Component* child, LayoutNodeProvider* provider) {
                              switch (child->coreType())
                              {
                                  case LayoutComponentBase::typeKey:
                                  case NestedArtboardLayoutBase::typeKey:
                                  case ArtboardComponentListBase::typeKey:
                                      break;
                                  default:
                                      provider->syncStyleChanges();
                                      break;
                              }
                          });
}

void LayoutComponent::clearLayoutChildren()
{
    YGNode& ourNode = m_layoutData->node;
    YGNodeRemoveAllChildren(&ourNode);
}

void LayoutComponent::syncLayoutChildren()
{
    YGNode& ourNode = m_layoutData->node;
    YGNodeRemoveAllChildren(&ourNode);
    int index = 0;
    forEachLayoutProvider(
        this,
        [&ourNode, &index](Component*, LayoutNodeProvider* provider) {
            for (size_t i = 0; i < provider->numLayoutNodes(); i++)
            {
                auto* node = static_cast<YGNode*>(provider->layoutNode((int)i));
                if (node != nullptr)
                {
                    ourNode.insertChild(node, index++);
                    node->setOwner(&ourNode);
                    ourNode.markDirtyAndPropagate();
                }
            }
        });
    markLayoutNodeDirty();
}

void LayoutComponent::propagateSize() { propagateSizeToChildren(this); }

void LayoutComponent::propagateSizeToChildren(ContainerComponent* component)
{
    if (isHidden())
    {
        return;
    }
    for (auto child : component->children())
    {
        // Don't content-size nested layouts, or containers transparent to
        // layout (groups/Solos) — their contents are free content or
        // participants, never content-sized from here.
        if (child->is<LayoutComponent>() || isTransparentLayoutContainer(child))
        {
            continue;
        }
        // A participating child owns its own layout node and is sized by the
        // layout engine, not by controlSize — skip it.
        if (LayoutNodeProvider::from(child) != nullptr)
        {
            continue;
        }
        auto sizeableChild = IntrinsicallySizeable::from(child);
        if (sizeableChild != nullptr && m_style != nullptr)
        {
            LayoutScaleType widthScaleType = m_style->widthScaleType();
            LayoutScaleType heightScaleType = m_style->heightScaleType();
            sizeableChild->controlSize(
                Vec2D(m_layout.width(), m_layout.height()),
                widthScaleType,
                heightScaleType,
                actualDirection());

            if (!sizeableChild->shouldPropagateSizeToChildren())
            {
                // Do not propagate to its children
                continue;
            }
        }
        if (child->is<ContainerComponent>())
        {
            propagateSizeToChildren(child->as<ContainerComponent>());
        }
    }
}

void LayoutComponent::calculateLayoutInternal(float availableWidth,
                                              float availableHeight)
{
    auto actualAvailWidth =
        std::isnan(availableWidth) && m_style && m_style->intrinsicallySized()
            ? availableWidth
            : width();
    auto actualAvailHeight =
        std::isnan(availableHeight) && m_style && m_style->intrinsicallySized()
            ? availableHeight
            : height();
    YGNodeCalculateLayout(&m_layoutData->node,
                          actualAvailWidth,
                          actualAvailHeight,
                          YGDirection::YGDirectionInherit);
}

bool LayoutComponent::styleDisplayHidden() const
{
    if (m_style == nullptr)
    {
        return false;
    }
    return m_style->display() == YGDisplayNone;
}

LayoutDirection LayoutComponent::actualDirection()
{
    if (m_style == nullptr)
    {
        return m_inheritedDirection;
    }
    switch (m_style->direction())
    {
        case YGDirectionLTR:
            return LayoutDirection::ltr;
        case YGDirectionRTL:
            return LayoutDirection::rtl;
        default:
            return m_inheritedDirection;
    }
}

void LayoutComponent::onDirty(ComponentDirt value)
{
    Super::onDirty(value);
    if ((value & ComponentDirt::WorldTransform) ==
            ComponentDirt::WorldTransform &&
        clip())
    {
        addDirt(ComponentDirt::Path);
    }
}

static Layout layoutFromYoga(const YGLayout& layout)
{
    return Layout(layout.position[YGEdgeLeft],
                  layout.position[YGEdgeTop],
                  layout.dimensions[YGDimensionWidth],
                  layout.dimensions[YGDimensionHeight]);
}

static LayoutPadding layoutPaddingFromYoga(const YGLayout& layout)
{
    return LayoutPadding(layout.padding[YGEdgeLeft],
                         layout.padding[YGEdgeTop],
                         layout.padding[YGEdgeRight],
                         layout.padding[YGEdgeBottom]);
}

void LayoutComponent::updateLayoutBounds(bool animate)
{
    YGNode& node = m_layoutData->node;
    bool updated = node.getHasNewLayout();
    if (!updated)
    {
        return;
    }
    node.setHasNewLayout(false);

    forEachLayoutProvider(this,
                          [animate](Component*, LayoutNodeProvider* provider) {
                              provider->updateLayoutBounds(animate);
                          });

    const auto& yogaLayout = node.getLayout();
    Layout newLayout = layoutFromYoga(yogaLayout);
    m_layoutPadding = layoutPaddingFromYoga(yogaLayout);

    if (m_justAddedToHost)
    {
        m_justAddedToHost = false;
        // In cases were we have a host (ie, Component List, etc), we
        // don't want to animate the x/y/width/height because the initial
        // x/y/width/height will have been 0,0,0,0 within the parent host.
        //
        // Instead, we want to the position/size to start at whatever this
        // layout's computed position is within its host.
        //
        // We'll keep this as an entirely seperate code path because
        // this may be useful in adding support for animate in/out of
        // items in an ArtboardHost.
        m_layout = newLayout;
        auto animationData = currentAnimationData();
        animationData->from = newLayout;
        animationData->to = newLayout;
        animationData->elapsedSeconds = 0.0f;
        propagateSize();
        markWorldTransformDirty();
        m_forceUpdateLayoutBounds = false;
        return;
    }

    if (animate && animates())
    {
        auto animationData = currentAnimationData();
        if (newLayout != animationData->to || m_forceUpdateLayoutBounds)
        {
            if (animationData->elapsedSeconds != 0.0f)
            {
                if (m_isSmoothingAnimation)
                {
                    // we were already smoothening.
                    m_animationDataA.copy(m_animationDataB);
                }
                m_isSmoothingAnimation = true;
            }
            else
            {
                m_isSmoothingAnimation = false;
            }
            animationData = currentAnimationData();
            animationData->from = m_layout;
            animationData->to = newLayout;
            animationData->elapsedSeconds = 0.0f;
            propagateSize();
            markWorldTransformDirty();
        }
    }
    else if (newLayout != m_layout || m_forceUpdateLayoutBounds)
    {
        if (m_layout.width() != newLayout.width() ||
            m_layout.height() != newLayout.height())
        {
            // Width changed, we need to rebuild the path.
            addDirt(ComponentDirt::Path);
        }
        m_animationDataA.to = m_layout = newLayout;

        propagateSize();
        markWorldTransformDirty();
    }
    m_forceUpdateLayoutBounds = false;
}

bool LayoutComponent::advanceComponent(float elapsedSeconds, AdvanceFlags flags)
{
    if ((flags & AdvanceFlags::NewFrame) != AdvanceFlags::NewFrame ||
        isCollapsed())
    {
        return false;
    }

    return applyInterpolation(elapsedSeconds,
                              (flags & AdvanceFlags::Animate) ==
                                      AdvanceFlags::Animate ||
                                  (flags & AdvanceFlags::AdvanceNested) ==
                                      AdvanceFlags::AdvanceNested);
}

bool LayoutComponent::animates()
{
    if (m_style == nullptr)
    {
        return false;
    }
    return m_style->animationStyle() != LayoutAnimationStyle::none &&
           interpolation() != LayoutStyleInterpolation::hold &&
           interpolationTime() > 0;
}

LayoutAnimationStyle LayoutComponent::animationStyle()
{
    if (m_style == nullptr)
    {
        return LayoutAnimationStyle::none;
    }
    return m_style->animationStyle();
}

KeyFrameInterpolator* LayoutComponent::interpolator()
{
    if (m_style == nullptr)
    {
        return nullptr;
    }
    switch (m_style->animationStyle())
    {
        case LayoutAnimationStyle::inherit:
            return m_inheritedInterpolator != nullptr ? m_inheritedInterpolator
                                                      : m_style->interpolator();
        case LayoutAnimationStyle::custom:
            return m_style->interpolator();
        default:
            return nullptr;
    }
}

LayoutStyleInterpolation LayoutComponent::interpolation()
{
    auto defaultInterpolation = LayoutStyleInterpolation::hold;
    if (m_style == nullptr)
    {
        return defaultInterpolation;
    }
    switch (m_style->animationStyle())
    {
        case LayoutAnimationStyle::inherit:
            return m_inheritedInterpolation;
        case LayoutAnimationStyle::custom:
            return m_style->interpolation();
        default:
            return defaultInterpolation;
    }
}

float LayoutComponent::interpolationTime()
{
    if (m_style == nullptr)
    {
        return 0;
    }
    switch (m_style->animationStyle())
    {
        case LayoutAnimationStyle::inherit:
            return m_inheritedInterpolationTime;
        case LayoutAnimationStyle::custom:
            return m_style->interpolationTime();
        default:
            return 0;
    }
}

bool LayoutComponent::cascadeLayoutStyle(
    LayoutStyleInterpolation inheritedInterpolation,
    KeyFrameInterpolator* inheritedInterpolator,
    float inheritedInterpolationTime,
    LayoutDirection direction)
{
    bool updated = false;
    if (m_style != nullptr &&
        m_style->animationStyle() == LayoutAnimationStyle::inherit)
    {
        updated = setInheritedInterpolation(inheritedInterpolation,
                                            inheritedInterpolator,
                                            inheritedInterpolationTime);
    }
    else
    {
        clearInheritedInterpolation();
    }
    auto oldDirection = m_inheritedDirection;
    if (direction == LayoutDirection::inherit ||
        (m_style != nullptr && m_style->direction() != YGDirectionInherit))
    {
        m_inheritedDirection = LayoutDirection::inherit;
    }
    else
    {
        m_inheritedDirection = direction;
    }
    if (m_inheritedDirection != oldDirection)
    {
        markLayoutNodeDirty(true);
        addDirt(ComponentDirt::Path);
        updated = true;
    }
    forEachLayoutProvider(this,
                          [this](Component*, LayoutNodeProvider* provider) {
                              provider->cascadeLayoutStyle(interpolation(),
                                                           interpolator(),
                                                           interpolationTime(),
                                                           actualDirection());
                          });
    return updated;
}

bool LayoutComponent::setInheritedInterpolation(
    LayoutStyleInterpolation inheritedInterpolation,
    KeyFrameInterpolator* inheritedInterpolator,
    float inheritedInterpolationTime)
{
    if (inheritedInterpolation != m_inheritedInterpolation ||
        inheritedInterpolator != m_inheritedInterpolator ||
        inheritedInterpolationTime != m_inheritedInterpolationTime)
    {
        m_inheritedInterpolation = inheritedInterpolation;
        m_inheritedInterpolator = inheritedInterpolator;
        m_inheritedInterpolationTime = inheritedInterpolationTime;
        return true;
    }
    return false;
}

void LayoutComponent::clearInheritedInterpolation()
{
    m_inheritedInterpolation = LayoutStyleInterpolation::hold;
    m_inheritedInterpolator = nullptr;
    m_inheritedInterpolationTime = 0;
}

LayoutAnimationData* LayoutComponent::currentAnimationData()
{
    return m_isSmoothingAnimation ? &m_animationDataB : &m_animationDataA;
}

void LayoutAnimationData::copy(const LayoutAnimationData& source)
{
    from = source.from;
    to = source.to;
    elapsedSeconds = source.elapsedSeconds;
}

bool LayoutComponent::applyInterpolation(float elapsedSeconds, bool animate)
{
    auto animationData = currentAnimationData();
    if (!animate || !animates() || m_style == nullptr ||
        animationData->to == m_layout)
    {
        return false;
    }
    if (m_isSmoothingAnimation)
    {
        float f = std::fmin(1.0f,
                            interpolationTime() > 0.0f
                                ? m_animationDataA.elapsedSeconds /
                                      interpolationTime()
                                : 1.0f);

        if (interpolation() != LayoutStyleInterpolation::linear &&
            interpolator() != nullptr)
        {
            f = interpolator()->transform(f);
        }
        m_animationDataB.from = m_animationDataA.interpolate(f);
        if (f == 1.0f)
        {
            m_animationDataA.copy(m_animationDataB);
            m_isSmoothingAnimation = false;
        }
        else
        {
            m_animationDataA.elapsedSeconds += elapsedSeconds;
        }
    }

    if ((animationData = currentAnimationData())->elapsedSeconds >=
        interpolationTime())
    {
        Layout newLayout = animationData->to;
        if (m_layout.width() != newLayout.width() ||
            m_layout.height() != newLayout.height())
        {
            addDirt(ComponentDirt::Path);
        }
        m_layout = animationData->to;

        if (m_isSmoothingAnimation)
        {
            m_isSmoothingAnimation = false;
            m_animationDataA.copy(m_animationDataB);
            m_animationDataA.elapsedSeconds = m_animationDataB.elapsedSeconds =
                0.0f;
        }
        else
        {
            m_animationDataA.elapsedSeconds = 0.0f;
        }
        propagateSize();
        markWorldTransformDirty();

        return false;
    }
    float f =
        std::fmin(1.0f,
                  interpolationTime() > 0
                      ? animationData->elapsedSeconds / interpolationTime()
                      : 1.0f);
    if (interpolation() != LayoutStyleInterpolation::linear &&
        interpolator() != nullptr)
    {
        f = interpolator()->transform(f);
    }

    auto current = animationData->interpolate(f);
    if (m_layout != current)
    {
        bool sizeChanged = m_layout.width() != current.width() ||
                           m_layout.height() != current.height();
        m_layout = current;
        if (sizeChanged)
        {
            propagateSize();
        }
        markWorldTransformDirty();
    }

    animationData->elapsedSeconds += elapsedSeconds;
    if (f != 1)
    {
        // Do we really need to mark the layout node dirty!!??
        markLayoutNodeDirty();
        return true;
    }
    return false;
}

void LayoutComponent::interruptAnimation()
{
    if (animates())
    {
        m_layout = currentAnimationData()->to;
        propagateSize();
    }
}

void LayoutComponent::markLayoutNodeDirty(bool shouldForceUpdateLayoutBounds)
{
    if (shouldForceUpdateLayoutBounds == true)
    {
        m_forceUpdateLayoutBounds = shouldForceUpdateLayoutBounds;
    }
    m_layoutData->node.markDirtyAndPropagate();
    artboard()->markLayoutDirty(this);
}

void LayoutComponent::markLayoutStyleDirty()
{
    clearInheritedInterpolation();
    addDirt(ComponentDirt::LayoutStyle);
    if (artboard() != this)
    {
        artboard()->markLayoutStyleDirty();
    }
}

void LayoutComponent::positionTypeChanged()
{
    if (m_style == nullptr)
    {
        return;
    }
    if (m_style->positionType() == YGPositionTypeAbsolute)
    {
        // Preserve computed position only if left/top were not explicitly keyed
        // this frame. If keyed, honor those values and only set units
        // appropriately.
        if (!m_positionLeftChanged)
        {
            m_style->positionLeft(layoutBounds().left());
        }
        if (!m_positionTopChanged)
        {
            m_style->positionTop(layoutBounds().top());
        }
        m_style->positionRight(0);
        m_style->positionBottom(0);
        m_style->positionLeftUnitsValue(YGUnitPoint);
        m_style->positionTopUnitsValue(YGUnitPoint);
        m_style->positionRightUnitsValue(YGUnitUndefined);
        m_style->positionBottomUnitsValue(YGUnitUndefined);
    }
    else
    {
        m_style->positionLeft(0);
        m_style->positionTop(0);
        m_style->positionRight(0);
        m_style->positionBottom(0);
        m_style->positionLeftUnitsValue(YGUnitUndefined);
        m_style->positionTopUnitsValue(YGUnitUndefined);
        m_style->positionRightUnitsValue(YGUnitUndefined);
        m_style->positionBottomUnitsValue(YGUnitUndefined);
    }
    markLayoutNodeDirty();
}

void LayoutComponent::scaleTypeChanged()
{
    if (m_style == nullptr)
    {
        return;
    }
    m_style->intrinsicallySizedValue(
        m_style->widthScaleType() == LayoutScaleType::hug ||
        m_style->heightScaleType() == LayoutScaleType::hug);
    markLayoutNodeDirty();
}

void LayoutComponent::displayChanged()
{
    if (m_style == nullptr)
    {
        return;
    }
    propagateCollapse(isCollapsed());
    markLayoutNodeDirty();
}

void LayoutComponent::syncChildProviderStyles()
{
    forEachLayoutProvider(this, [](Component*, LayoutNodeProvider* provider) {
        provider->syncStyleChanges();
        provider->markLayoutNodeDirty();
    });
}

void LayoutComponent::flexDirectionChanged()
{
    markLayoutNodeDirty();
    syncChildProviderStyles();
}

// Switching grid/flex/stack changes how each child's style maps to yoga.
void LayoutComponent::layoutTypeChanged()
{
    markLayoutNodeDirty();
    syncChildProviderStyles();
}

void LayoutComponent::directionChanged()
{
    markLayoutStyleDirty();
    markLayoutNodeDirty(true);
}
#else
LayoutComponent::LayoutComponent() :
    m_layoutData(new LayoutData()), m_proxy(this)
{}

float LayoutComponent::gapHorizontal() { return 0; }
float LayoutComponent::gapVertical() { return 0; }

void LayoutComponent::drawProxy(Renderer* renderer) {}

void LayoutComponent::draw(Renderer* renderer) {}

void LayoutComponent::updateRenderPath() {}

Vec2D LayoutComponent::measureLayout(float width,
                                     LayoutMeasureMode widthMode,
                                     float height,
                                     LayoutMeasureMode heightMode)
{
    return Vec2D();
}

bool LayoutComponent::advanceComponent(float elapsedSeconds, AdvanceFlags flags)
{
    return false;
}

void LayoutComponent::markLayoutNodeDirty(bool shouldForceUpdateLayoutBounds) {}
void LayoutComponent::markLayoutStyleDirty() {}
void LayoutComponent::onDirty(ComponentDirt value) {}
bool LayoutComponent::mainAxisIsRow() { return true; }

bool LayoutComponent::mainAxisIsColumn() { return false; }
bool LayoutComponent::isStackContainer() { return false; }
void LayoutComponent::calculateLayoutInternal(float availableWidth,
                                              float availableHeight)
{}
#endif

LayoutComponent::~LayoutComponent()
{
    if (artboard() != nullptr)
    {
        artboard()->cleanLayout(this);
    }
#ifdef WITH_RIVE_TOOLS
    m_layoutData->unref();
#else
    delete m_layoutData;
#endif
}

void LayoutComponent::clipChanged()
{
    markLayoutNodeDirty();
    addDirt(ComponentDirt::Path);
}
void LayoutComponent::widthChanged() { markLayoutNodeDirty(); }
void LayoutComponent::heightChanged() { markLayoutNodeDirty(); }
void LayoutComponent::styleIdChanged() { markLayoutNodeDirty(); }
void LayoutComponent::fractionalWidthChanged() { markLayoutNodeDirty(); }
void LayoutComponent::fractionalHeightChanged() { markLayoutNodeDirty(); }

ShapePaintPath* LayoutComponent::worldPath() { return &m_worldPath; }
ShapePaintPath* LayoutComponent::localPath() { return &m_localPath; }
ShapePaintPath* LayoutComponent::localClockwisePath() { return &m_localPath; }

Component* LayoutComponent::pathBuilder() { return this; }