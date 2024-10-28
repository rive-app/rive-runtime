#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/artboard.hpp"
#include "rive/drawable.hpp"
#include "rive/factory.hpp"
#include "rive/intrinsically_sizeable.hpp"
#include "rive/layout_component.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/node.hpp"
#include "rive/math/aabb.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/nested_artboard_layout.hpp"
#ifdef WITH_RIVE_LAYOUT
#include "rive/transform_component.hpp"
#include "yoga/YGEnums.h"
#include "yoga/YGFloatOptional.h"
#endif
#include <vector>

using namespace rive;

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

void LayoutComponent::drawProxy(Renderer* renderer)
{
    if (clip())
    {
        renderer->save();
        renderer->clipPath(m_clipPath.get());
    }
    renderer->save();
    renderer->transform(worldTransform());
    for (auto shapePaint : m_ShapePaints)
    {
        if (!shapePaint->isVisible())
        {
            continue;
        }
        if (shapePaint->is<Fill>())
        {
            shapePaint->draw(renderer,
                             m_backgroundPath.get(),
                             &m_backgroundRect->rawPath());
        }
    }
    renderer->restore();
}

void LayoutComponent::draw(Renderer* renderer)
{
    // Restore clip before drawing stroke so we don't clip the stroke
    if (clip())
    {
        renderer->restore();
    }
    renderer->save();
    renderer->transform(worldTransform());
    for (auto shapePaint : m_ShapePaints)
    {
        if (!shapePaint->isVisible())
        {
            continue;
        }
        if (shapePaint->is<Stroke>())
        {
            shapePaint->draw(renderer,
                             m_backgroundPath.get(),
                             &m_backgroundRect->rawPath());
        }
    }
    renderer->restore();
}

Core* LayoutComponent::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

void LayoutComponent::updateRenderPath()
{
    m_backgroundRect->width(m_layout.width());
    m_backgroundRect->height(m_layout.height());
    if (style() != nullptr)
    {
        m_backgroundRect->linkCornerRadius(style()->linkCornerRadius());
        m_backgroundRect->cornerRadiusTL(style()->cornerRadiusTL());
        m_backgroundRect->cornerRadiusTR(style()->cornerRadiusTR());
        m_backgroundRect->cornerRadiusBL(style()->cornerRadiusBL());
        m_backgroundRect->cornerRadiusBR(style()->cornerRadiusBR());
    }
    m_backgroundRect->update(ComponentDirt::Path);

    m_backgroundPath->rewind();
    m_backgroundRect->rawPath().addTo(m_backgroundPath.get());

    RawPath clipPath;
    clipPath.addPath(m_backgroundRect->rawPath(), &m_WorldTransform);
    m_clipPath =
        artboard()->factory()->makeRenderPath(clipPath, FillRule::nonZero);
    for (auto shapePaint : m_ShapePaints)
    {
        if (!shapePaint->isVisible())
        {
            continue;
        }
        if (shapePaint->is<Stroke>())
        {
            shapePaint->as<Stroke>()->invalidateEffects();
        }
    }
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
        Mat2D parentWorld =
            parent()->is<WorldTransformComponent>()
                ? (parent()->as<WorldTransformComponent>())->worldTransform()
                : Mat2D();
        auto location = Vec2D(m_layout.left(), m_layout.top());
        if (parent()->is<Artboard>())
        {
            auto art = parent()->as<Artboard>();
            location -= Vec2D(art->layoutWidth() * art->originX(),
                              art->layoutHeight() * art->originY());
        }
        auto transform = Mat2D::fromTranslation(location);
        m_WorldTransform = Mat2D::multiply(parentWorld, transform);
        updateConstraints();
    }
    if (hasDirt(value, ComponentDirt::Path))
    {
        updateRenderPath();
    }
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

#ifdef WITH_RIVE_LAYOUT
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

    return StatusCode::Ok;
}

StatusCode LayoutComponent::onAddedClean(CoreContext* context)
{
    auto code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    artboard()->markLayoutDirty(this);
    markLayoutStyleDirty();
    m_backgroundPath = artboard()->factory()->makeEmptyRenderPath();
    m_clipPath = artboard()->factory()->makeEmptyRenderPath();
    m_backgroundRect->originX(0);
    m_backgroundRect->originY(0);
    syncLayoutChildren();
    return StatusCode::Ok;
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

bool LayoutComponent::isLeaf()
{
    for (auto child : children())
    {
        if (child->is<LayoutComponent>() || child->is<NestedArtboardLayout>())
        {
            return false;
        }
    }
    return true;
}

void LayoutComponent::syncStyle()
{
    if (m_style == nullptr)
    {
        return;
    }
    YGNode& ygNode = layoutNode();
    YGStyle& ygStyle = layoutStyle();
    if (m_style->intrinsicallySized() && isLeaf())
    {
        ygNode.setContext(this);
        ygNode.setMeasureFunc(measureFunc);
    }
    else
    {
        ygNode.setMeasureFunc(nullptr);
    }

    auto realWidth = width();
    auto realWidthUnits = m_style->widthUnits();
    auto realWidthScaleType = m_style->widthScaleType();
    auto realHeight = height();
    auto realHeightUnits = m_style->heightUnits();
    auto realHeightScaleType = m_style->heightScaleType();
    auto parentIsRow =
        layoutParent() != nullptr ? layoutParent()->mainAxisIsRow() : true;

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
        parentIsRow = m_parentIsRow;

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
    ygStyle.dimensions()[YGDimensionWidth] = YGValue{realWidth, realWidthUnits};
    ygStyle.dimensions()[YGDimensionHeight] =
        YGValue{realHeight, realHeightUnits};

    switch (realWidthScaleType)
    {
        case LayoutScaleType::fixed:
            if (parentIsRow)
            {
                ygStyle.flexGrow() = YGFloatOptional(0);
                ygStyle.flexShrink() = YGFloatOptional(0);
                ygStyle.flexBasis() = YGValue{m_style->flexBasis(), YGUnitAuto};
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
                ygStyle.flexBasis() =
                    YGValue{m_style->flexBasis(), m_style->flexBasisUnits()};
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
                ygStyle.flexBasis() = YGValue{m_style->flexBasis(), YGUnitAuto};
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
                ygStyle.flexBasis() = YGValue{m_style->flexBasis(), YGUnitAuto};
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
                ygStyle.flexBasis() =
                    YGValue{m_style->flexBasis(), m_style->flexBasisUnits()};
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
                ygStyle.flexBasis() = YGValue{m_style->flexBasis(), YGUnitAuto};
            }
            else
            {
                ygStyle.alignSelf() = YGAlignAuto;
            }
            break;
        default:
            break;
    }

    bool isRowForAlignment = mainAxisIsRow();
    switch (m_style->alignmentType())
    {
        case LayoutAlignmentType::topLeft:
        case LayoutAlignmentType::topCenter:
        case LayoutAlignmentType::topRight:
        case LayoutAlignmentType::spaceBetweenStart:
            if (isRowForAlignment)
            {
                ygStyle.alignItems() = YGAlignFlexStart;
                ygStyle.alignContent() = YGAlignFlexStart;
            }
            else
            {
                ygStyle.justifyContent() = YGJustifyFlexStart;
            }
            break;
        case LayoutAlignmentType::centerLeft:
        case LayoutAlignmentType::center:
        case LayoutAlignmentType::centerRight:
        case LayoutAlignmentType::spaceBetweenCenter:
            if (isRowForAlignment)
            {
                ygStyle.alignItems() = YGAlignCenter;
                ygStyle.alignContent() = YGAlignCenter;
            }
            else
            {
                ygStyle.justifyContent() = YGJustifyCenter;
            }
            break;
        case LayoutAlignmentType::bottomLeft:
        case LayoutAlignmentType::bottomCenter:
        case LayoutAlignmentType::bottomRight:
        case LayoutAlignmentType::spaceBetweenEnd:
            if (isRowForAlignment)
            {
                ygStyle.alignItems() = YGAlignFlexEnd;
                ygStyle.alignContent() = YGAlignFlexEnd;
            }
            else
            {
                ygStyle.justifyContent() = YGJustifyFlexEnd;
            }
            break;
    }
    switch (m_style->alignmentType())
    {
        case LayoutAlignmentType::topLeft:
        case LayoutAlignmentType::centerLeft:
        case LayoutAlignmentType::bottomLeft:
            if (isRowForAlignment)
            {
                ygStyle.justifyContent() = YGJustifyFlexStart;
            }
            else
            {
                ygStyle.alignItems() = YGAlignFlexStart;
                ygStyle.alignContent() = YGAlignFlexStart;
            }
            break;
        case LayoutAlignmentType::topCenter:
        case LayoutAlignmentType::center:
        case LayoutAlignmentType::bottomCenter:
            if (isRowForAlignment)
            {
                ygStyle.justifyContent() = YGJustifyCenter;
            }
            else
            {
                ygStyle.alignItems() = YGAlignCenter;
                ygStyle.alignContent() = YGAlignCenter;
            }
            break;
        case LayoutAlignmentType::topRight:
        case LayoutAlignmentType::centerRight:
        case LayoutAlignmentType::bottomRight:
            if (isRowForAlignment)
            {
                ygStyle.justifyContent() = YGJustifyFlexEnd;
            }
            else
            {
                ygStyle.alignItems() = YGAlignFlexEnd;
                ygStyle.alignContent() = YGAlignFlexEnd;
            }
            break;
        case LayoutAlignmentType::spaceBetweenStart:
        case LayoutAlignmentType::spaceBetweenCenter:
        case LayoutAlignmentType::spaceBetweenEnd:
            ygStyle.justifyContent() = YGJustifySpaceBetween;
            break;
    }

    ygStyle.minDimensions()[YGDimensionWidth] =
        YGValue{m_style->minWidth(), m_style->minWidthUnits()};
    ygStyle.minDimensions()[YGDimensionHeight] =
        YGValue{m_style->minHeight(), m_style->minHeightUnits()};
    ygStyle.maxDimensions()[YGDimensionWidth] =
        YGValue{m_style->maxWidth(), m_style->maxWidthUnits()};
    ygStyle.maxDimensions()[YGDimensionHeight] =
        YGValue{m_style->maxHeight(), m_style->maxHeightUnits()};
    ygStyle.gap()[YGGutterColumn] =
        YGValue{m_style->gapHorizontal(), m_style->gapHorizontalUnits()};
    ygStyle.gap()[YGGutterRow] =
        YGValue{m_style->gapVertical(), m_style->gapVerticalUnits()};
    ygStyle.border()[YGEdgeLeft] =
        YGValue{m_style->borderLeft(), m_style->borderLeftUnits()};
    ygStyle.border()[YGEdgeRight] =
        YGValue{m_style->borderRight(), m_style->borderRightUnits()};
    ygStyle.border()[YGEdgeTop] =
        YGValue{m_style->borderTop(), m_style->borderTopUnits()};
    ygStyle.border()[YGEdgeBottom] =
        YGValue{m_style->borderBottom(), m_style->borderBottomUnits()};
    ygStyle.margin()[YGEdgeLeft] =
        YGValue{m_style->marginLeft(), m_style->marginLeftUnits()};
    ygStyle.margin()[YGEdgeRight] =
        YGValue{m_style->marginRight(), m_style->marginRightUnits()};
    ygStyle.margin()[YGEdgeTop] =
        YGValue{m_style->marginTop(), m_style->marginTopUnits()};
    ygStyle.margin()[YGEdgeBottom] =
        YGValue{m_style->marginBottom(), m_style->marginBottomUnits()};
    ygStyle.padding()[YGEdgeLeft] =
        YGValue{m_style->paddingLeft(), m_style->paddingLeftUnits()};
    ygStyle.padding()[YGEdgeRight] =
        YGValue{m_style->paddingRight(), m_style->paddingRightUnits()};
    ygStyle.padding()[YGEdgeTop] =
        YGValue{m_style->paddingTop(), m_style->paddingTopUnits()};
    ygStyle.padding()[YGEdgeBottom] =
        YGValue{m_style->paddingBottom(), m_style->paddingBottomUnits()};
    ygStyle.position()[YGEdgeLeft] =
        YGValue{m_style->positionLeft(), m_style->positionLeftUnits()};
    ygStyle.position()[YGEdgeRight] =
        YGValue{m_style->positionRight(), m_style->positionRightUnits()};
    ygStyle.position()[YGEdgeTop] =
        YGValue{m_style->positionTop(), m_style->positionTopUnits()};
    ygStyle.position()[YGEdgeBottom] =
        YGValue{m_style->positionBottom(), m_style->positionBottomUnits()};

    ygStyle.display() = m_style->display();
    ygStyle.positionType() = m_style->positionType();
    ygStyle.flex() = YGFloatOptional(m_style->flex());
    ygStyle.flexDirection() = m_style->flexDirection();
    ygStyle.flexWrap() = m_style->flexWrap();

    ygNode.setStyle(ygStyle);
}

void LayoutComponent::syncLayoutChildren()
{
    YGNode& ourNode = layoutNode();
    YGNodeRemoveAllChildren(&ourNode);
    int index = 0;
    for (auto child : children())
    {
        YGNode* node = nullptr;
        switch (child->coreType())
        {
            case LayoutComponentBase::typeKey:
                node = &child->as<LayoutComponent>()->layoutNode();
                break;
            case NestedArtboardLayoutBase::typeKey:
                node = static_cast<YGNode*>(
                    child->as<NestedArtboardLayout>()->layoutNode());
                break;
        }
        if (node != nullptr)
        {
            ourNode.insertChild(node, index++);
            node->setOwner(&ourNode);
            ourNode.markDirtyAndPropagate();
        }
    }
    markLayoutNodeDirty();
}

void LayoutComponent::propagateSize() { propagateSizeToChildren(this); }

void LayoutComponent::propagateSizeToChildren(ContainerComponent* component)
{
    for (auto child : component->children())
    {
        if (child->is<LayoutComponent>() ||
            child->coreType() == NodeBase::typeKey)
        {
            continue;
        }
        auto sizeableChild = IntrinsicallySizeable::from(child);
        if (sizeableChild != nullptr)
        {
            sizeableChild->controlSize(
                Vec2D(m_layout.width(), m_layout.height()));

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

void LayoutComponent::calculateLayout()
{
    YGNodeCalculateLayout(&layoutNode(),
                          width(),
                          height(),
                          YGDirection::YGDirectionInherit);
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

Layout::Layout(const YGLayout& layout) :
    m_left(layout.position[YGEdgeLeft]),
    m_top(layout.position[YGEdgeTop]),
    m_width(layout.dimensions[YGDimensionWidth]),
    m_height(layout.dimensions[YGDimensionHeight])
{}

void LayoutComponent::updateLayoutBounds(bool animate)
{
    YGNode& node = layoutNode();
    bool updated = node.getHasNewLayout();
    if (!updated)
    {
        return;
    }
    node.setHasNewLayout(false);

    for (auto child : children())
    {
        if (child->is<LayoutComponent>())
        {
            child->as<LayoutComponent>()->updateLayoutBounds(animate);
        }
        else if (child->is<NestedArtboardLayout>())
        {
            child->as<NestedArtboardLayout>()->updateLayoutBounds(animate);
        }
    }

    Layout newLayout(node.getLayout());

    if (animate && animates())
    {
        auto animationData = currentAnimationData();
        if (newLayout != animationData->to)
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
    else if (newLayout != m_layout)
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
}

bool LayoutComponent::advanceComponent(float elapsedSeconds, bool animate)
{
    return applyInterpolation(elapsedSeconds, animate);
}

bool LayoutComponent::animates()
{
    if (m_style == nullptr)
    {
        return false;
    }
    return m_style->positionType() == YGPositionType::YGPositionTypeRelative &&
           m_style->animationStyle() != LayoutAnimationStyle::none &&
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

void LayoutComponent::cascadeAnimationStyle(
    LayoutStyleInterpolation inheritedInterpolation,
    KeyFrameInterpolator* inheritedInterpolator,
    float inheritedInterpolationTime)
{
    if (m_style != nullptr &&
        m_style->animationStyle() == LayoutAnimationStyle::inherit)
    {
        setInheritedInterpolation(inheritedInterpolation,
                                  inheritedInterpolator,
                                  inheritedInterpolationTime);
    }
    else
    {
        clearInheritedInterpolation();
    }
    for (auto child : children())
    {
        if (child->is<LayoutComponent>())
        {
            child->as<LayoutComponent>()->cascadeAnimationStyle(
                interpolation(),
                interpolator(),
                interpolationTime());
        }
    }
}

void LayoutComponent::setInheritedInterpolation(
    LayoutStyleInterpolation inheritedInterpolation,
    KeyFrameInterpolator* inheritedInterpolator,
    float inheritedInterpolationTime)
{
    m_inheritedInterpolation = inheritedInterpolation;
    m_inheritedInterpolator = inheritedInterpolator;
    m_inheritedInterpolationTime = inheritedInterpolationTime;
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

void LayoutComponent::markLayoutNodeDirty()
{
    layoutNode().markDirtyAndPropagate();
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
        m_style->positionLeft(layoutBounds().left());
        m_style->positionTop(layoutBounds().top());
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
    m_style->widthUnitsValue(m_style->widthScaleType() == LayoutScaleType::fixed
                                 ? YGUnitPoint
                                 : YGUnitAuto);
    m_style->heightUnitsValue(
        m_style->heightScaleType() == LayoutScaleType::fixed ? YGUnitPoint
                                                             : YGUnitAuto);
    m_style->intrinsicallySizedValue(
        m_style->widthScaleType() == LayoutScaleType::hug ||
        m_style->heightScaleType() == LayoutScaleType::hug);
    markLayoutNodeDirty();
}
#else
Vec2D LayoutComponent::measureLayout(float width,
                                     LayoutMeasureMode widthMode,
                                     float height,
                                     LayoutMeasureMode heightMode)
{
    return Vec2D();
}

bool LayoutComponent::advanceComponent(float elapsedSeconds, bool animate)
{
    return false;
}

void LayoutComponent::markLayoutNodeDirty() {}
void LayoutComponent::markLayoutStyleDirty() {}
void LayoutComponent::onDirty(ComponentDirt value) {}
bool LayoutComponent::mainAxisIsRow() { return true; }

bool LayoutComponent::mainAxisIsColumn() { return false; }
#endif

void LayoutComponent::clipChanged() { markLayoutNodeDirty(); }
void LayoutComponent::widthChanged() { markLayoutNodeDirty(); }
void LayoutComponent::heightChanged() { markLayoutNodeDirty(); }
void LayoutComponent::styleIdChanged() { markLayoutNodeDirty(); }
void LayoutComponent::fractionalWidthChanged() { markLayoutNodeDirty(); }
void LayoutComponent::fractionalHeightChanged() { markLayoutNodeDirty(); }
