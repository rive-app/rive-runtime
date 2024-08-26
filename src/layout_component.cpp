#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/artboard.hpp"
#include "rive/drawable.hpp"
#include "rive/factory.hpp"
#include "rive/intrinsically_sizeable.hpp"
#include "rive/layout_component.hpp"
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
            shapePaint->draw(renderer, m_backgroundPath.get(), &m_backgroundRect->rawPath());
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
            shapePaint->draw(renderer, m_backgroundPath.get(), &m_backgroundRect->rawPath());
        }
    }
    renderer->restore();
}

Core* LayoutComponent::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

void LayoutComponent::updateRenderPath()
{
    m_backgroundRect->width(m_layoutSizeWidth);
    m_backgroundRect->height(m_layoutSizeHeight);
    m_backgroundRect->linkCornerRadius(style()->linkCornerRadius());
    m_backgroundRect->cornerRadiusTL(style()->cornerRadiusTL());
    m_backgroundRect->cornerRadiusTR(style()->cornerRadiusTR());
    m_backgroundRect->cornerRadiusBL(style()->cornerRadiusBL());
    m_backgroundRect->cornerRadiusBR(style()->cornerRadiusBR());
    m_backgroundRect->update(ComponentDirt::Path);

    m_backgroundPath->rewind();
    m_backgroundRect->rawPath().addTo(m_backgroundPath.get());

    RawPath clipPath;
    clipPath.addPath(m_backgroundRect->rawPath(), &m_WorldTransform);
    m_clipPath = artboard()->factory()->makeRenderPath(clipPath, FillRule::nonZero);
}

void LayoutComponent::update(ComponentDirt value)
{
    Super::update(value);
    if (hasDirt(value, ComponentDirt::RenderOpacity))
    {
        propagateOpacity(childOpacity());
    }
    if (parent() != nullptr && hasDirt(value, ComponentDirt::WorldTransform))
    {
        Mat2D parentWorld = parent()->is<WorldTransformComponent>()
                                ? (parent()->as<WorldTransformComponent>())->worldTransform()
                                : Mat2D();
        auto location = Vec2D(m_layoutLocationX, m_layoutLocationY);
        if (parent()->is<Artboard>())
        {
            auto art = parent()->as<Artboard>();
            location -=
                Vec2D(art->layoutWidth() * art->originX(), art->layoutHeight() * art->originY());
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
            Vec2D measured = sizeableChild->measureLayout(width, widthMode, height, heightMode);
            size = Vec2D(std::max(size.x, measured.x), std::max(size.y, measured.y));
        }
    }
    return size;
}

bool LayoutComponent::mainAxisIsRow()
{
    return style()->flexDirection() == YGFlexDirectionRow ||
           style()->flexDirection() == YGFlexDirectionRowReverse;
}

bool LayoutComponent::mainAxisIsColumn()
{
    return style()->flexDirection() == YGFlexDirectionColumn ||
           style()->flexDirection() == YGFlexDirectionColumnReverse;
}

void LayoutComponent::syncStyle()
{
    if (m_style == nullptr)
    {
        return;
    }
    YGNode& ygNode = layoutNode();
    YGStyle& ygStyle = layoutStyle();
    if (m_style->intrinsicallySized())
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
    auto parentIsRow = layoutParent() != nullptr ? layoutParent()->mainAxisIsRow() : true;

    // If we have override width/height values, use those.
    // Currently we only use these for Artboards that are part of a NestedArtboardLayout
    // but perhaps there will be other use cases for overriding in the future?
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
                    realWidthScaleType = LayoutScaleType::fill;
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
                    realHeightScaleType = LayoutScaleType::fill;
                    break;
                default:
                    break;
            }
        }
    }
    ygStyle.dimensions()[YGDimensionWidth] = YGValue{realWidth, realWidthUnits};
    ygStyle.dimensions()[YGDimensionHeight] = YGValue{realHeight, realHeightUnits};

    switch (realWidthScaleType)
    {
        case LayoutScaleType::fixed:
            if (parentIsRow)
            {
                ygStyle.flexGrow() = YGFloatOptional(0);
            }
            break;
        case LayoutScaleType::fill:
            if (parentIsRow)
            {
                ygStyle.flexGrow() = YGFloatOptional(1);
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
            }
            break;
        case LayoutScaleType::fill:
            if (!parentIsRow)
            {
                ygStyle.flexGrow() = YGFloatOptional(1);
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
    ygStyle.gap()[YGGutterRow] = YGValue{m_style->gapVertical(), m_style->gapVerticalUnits()};
    ygStyle.border()[YGEdgeLeft] = YGValue{m_style->borderLeft(), m_style->borderLeftUnits()};
    ygStyle.border()[YGEdgeRight] = YGValue{m_style->borderRight(), m_style->borderRightUnits()};
    ygStyle.border()[YGEdgeTop] = YGValue{m_style->borderTop(), m_style->borderTopUnits()};
    ygStyle.border()[YGEdgeBottom] = YGValue{m_style->borderBottom(), m_style->borderBottomUnits()};
    ygStyle.margin()[YGEdgeLeft] = YGValue{m_style->marginLeft(), m_style->marginLeftUnits()};
    ygStyle.margin()[YGEdgeRight] = YGValue{m_style->marginRight(), m_style->marginRightUnits()};
    ygStyle.margin()[YGEdgeTop] = YGValue{m_style->marginTop(), m_style->marginTopUnits()};
    ygStyle.margin()[YGEdgeBottom] = YGValue{m_style->marginBottom(), m_style->marginBottomUnits()};
    ygStyle.padding()[YGEdgeLeft] = YGValue{m_style->paddingLeft(), m_style->paddingLeftUnits()};
    ygStyle.padding()[YGEdgeRight] = YGValue{m_style->paddingRight(), m_style->paddingRightUnits()};
    ygStyle.padding()[YGEdgeTop] = YGValue{m_style->paddingTop(), m_style->paddingTopUnits()};
    ygStyle.padding()[YGEdgeBottom] =
        YGValue{m_style->paddingBottom(), m_style->paddingBottomUnits()};
    ygStyle.position()[YGEdgeLeft] = YGValue{m_style->positionLeft(), m_style->positionLeftUnits()};
    ygStyle.position()[YGEdgeRight] =
        YGValue{m_style->positionRight(), m_style->positionRightUnits()};
    ygStyle.position()[YGEdgeTop] = YGValue{m_style->positionTop(), m_style->positionTopUnits()};
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
    auto ourNode = &layoutNode();
    YGNodeRemoveAllChildren(ourNode);
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
                node = static_cast<YGNode*>(child->as<NestedArtboardLayout>()->layoutNode());
                break;
        }
        if (node != nullptr)
        {
            // YGNodeInsertChild(ourNode, node, index++);
            ourNode->insertChild(node, index++);
            node->setOwner(ourNode);
            ourNode->markDirtyAndPropagate();
        }
    }
    markLayoutNodeDirty();
}

void LayoutComponent::propagateSize() { propagateSizeToChildren(this); }

void LayoutComponent::propagateSizeToChildren(ContainerComponent* component)
{
    for (auto child : component->children())
    {
        if (child->is<LayoutComponent>() || child->coreType() == NodeBase::typeKey)
        {
            continue;
        }
        auto sizeableChild = IntrinsicallySizeable::from(child);
        if (sizeableChild != nullptr)
        {
            sizeableChild->controlSize(Vec2D(m_layoutSizeWidth, m_layoutSizeHeight));
        }
        if (child->is<ContainerComponent>())
        {
            propagateSizeToChildren(child->as<ContainerComponent>());
        }
    }
}

void LayoutComponent::calculateLayout()
{
    YGNodeCalculateLayout(&layoutNode(), width(), height(), YGDirection::YGDirectionInherit);
}

void LayoutComponent::onDirty(ComponentDirt value)
{
    Super::onDirty(value);
    if ((value & ComponentDirt::WorldTransform) == ComponentDirt::WorldTransform && clip())
    {
        addDirt(ComponentDirt::Path);
    }
}

void LayoutComponent::updateLayoutBounds()
{
    auto node = &layoutNode();
    auto left = YGNodeLayoutGetLeft(node);
    auto top = YGNodeLayoutGetTop(node);
    auto width = YGNodeLayoutGetWidth(node);
    auto height = YGNodeLayoutGetHeight(node);

#ifdef DEBUG
    // Temporarily here to keep track of an issue.
    if (left != left || top != top || width != width || height != height)
    {
        fprintf(stderr,
                "Layout returned nan: %f %f %f %f | %p %s\n",
                left,
                top,
                width,
                height,
                YGNodeGetParent(node),
                name().c_str());
        return;
    }
#endif
    if (animates())
    {
        auto toBounds = m_animationData.toBounds;
        if (left != toBounds.left() || top != toBounds.top() || width != toBounds.width() ||
            height != toBounds.height())
        {
            m_animationData.fromBounds = AABB(m_layoutLocationX,
                                              m_layoutLocationY,
                                              m_layoutLocationX + this->width(),
                                              m_layoutLocationY + this->height());
            m_animationData.toBounds = AABB(left, top, left + width, top + height);
            if (m_animationData.elapsedSeconds > 0.1)
            {
                m_animationData.elapsedSeconds = 0;
            }
            propagateSize();
            markWorldTransformDirty();
        }
    }
    else

        if (left != m_layoutLocationX || top != m_layoutLocationY || width != m_layoutSizeWidth ||
            height != m_layoutSizeHeight)
    {
        if (m_layoutSizeWidth != width || m_layoutSizeHeight != height)
        {
            // Width changed, we need to rebuild the path.
            addDirt(ComponentDirt::Path);
        }
        m_layoutLocationX = left;
        m_layoutLocationY = top;
        m_layoutSizeWidth = width;
        m_layoutSizeHeight = height;

        propagateSize();
        markWorldTransformDirty();
    }
}

bool LayoutComponent::advance(double elapsedSeconds) { return applyInterpolation(elapsedSeconds); }

bool LayoutComponent::animates()
{
    if (m_style == nullptr)
    {
        return false;
    }
    return m_style->positionType() == YGPositionType::YGPositionTypeRelative &&
           m_style->animationStyle() != LayoutAnimationStyle::none &&
           interpolation() != LayoutStyleInterpolation::hold && interpolationTime() > 0;
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

void LayoutComponent::cascadeAnimationStyle(LayoutStyleInterpolation inheritedInterpolation,
                                            KeyFrameInterpolator* inheritedInterpolator,
                                            float inheritedInterpolationTime)
{
    if (m_style != nullptr && m_style->animationStyle() == LayoutAnimationStyle::inherit)
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
            child->as<LayoutComponent>()->cascadeAnimationStyle(interpolation(),
                                                                interpolator(),
                                                                interpolationTime());
        }
    }
}

void LayoutComponent::setInheritedInterpolation(LayoutStyleInterpolation inheritedInterpolation,
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

bool LayoutComponent::applyInterpolation(double elapsedSeconds)
{
    if (!animates() || m_style == nullptr || m_animationData.toBounds == layoutBounds())
    {
        return false;
    }

    if (m_animationData.elapsedSeconds >= interpolationTime())
    {
        m_layoutLocationX = m_animationData.toBounds.left();
        m_layoutLocationY = m_animationData.toBounds.top();

        float width = m_animationData.toBounds.width();
        float height = m_animationData.toBounds.height();
        if (width != m_layoutSizeWidth || height != m_layoutSizeHeight)
        {
            addDirt(ComponentDirt::Path);
        }
        m_layoutSizeWidth = width;
        m_layoutSizeHeight = height;

        m_animationData.elapsedSeconds = 0;
        propagateSize();
        markWorldTransformDirty();

        return false;
    }
    float f = 1;
    if (interpolationTime() > 0)
    {
        f = m_animationData.elapsedSeconds / interpolationTime();
    }
    bool needsAdvance = false;
    auto fromBounds = m_animationData.fromBounds;
    auto toBounds = m_animationData.toBounds;
    auto left = m_layoutLocationX;
    auto top = m_layoutLocationY;
    auto width = m_layoutSizeWidth;
    auto height = m_layoutSizeHeight;
    if (toBounds.left() != left || toBounds.top() != top)
    {
        if (interpolation() == LayoutStyleInterpolation::linear)
        {
            left = fromBounds.left() + f * (toBounds.left() - fromBounds.left());
            top = fromBounds.top() + f * (toBounds.top() - fromBounds.top());
        }
        else
        {
            if (interpolator() != nullptr)
            {
                left = interpolator()->transformValue(fromBounds.left(), toBounds.left(), f);
                top = interpolator()->transformValue(fromBounds.top(), toBounds.top(), f);
            }
        }
        needsAdvance = true;
        m_layoutLocationX = left;
        m_layoutLocationY = top;
    }
    if (toBounds.width() != width || toBounds.height() != height)
    {
        if (interpolation() == LayoutStyleInterpolation::linear)
        {
            width = fromBounds.width() + f * (toBounds.width() - fromBounds.width());
            height = fromBounds.height() + f * (toBounds.height() - fromBounds.height());
        }
        else
        {
            if (interpolator() != nullptr)
            {
                width = interpolator()->transformValue(fromBounds.width(), toBounds.width(), f);
                height = interpolator()->transformValue(fromBounds.height(), toBounds.height(), f);
            }
        }
        needsAdvance = true;
        m_layoutSizeWidth = width;
        m_layoutSizeHeight = height;
        addDirt(ComponentDirt::Path);
    }
    m_animationData.elapsedSeconds = m_animationData.elapsedSeconds + (float)elapsedSeconds;
    if (needsAdvance)
    {
        propagateSize();
        markWorldTransformDirty();
    }
    return needsAdvance;
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
#else
Vec2D LayoutComponent::measureLayout(float width,
                                     LayoutMeasureMode widthMode,
                                     float height,
                                     LayoutMeasureMode heightMode)
{
    return Vec2D();
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
