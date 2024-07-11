#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/artboard.hpp"
#include "rive/layout_component.hpp"
#include "rive/node.hpp"
#include "rive/math/aabb.hpp"
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
}

#ifdef WITH_RIVE_LAYOUT
LayoutComponent::LayoutComponent() : m_layoutData(std::unique_ptr<LayoutData>(new LayoutData()))
{
    layoutNode().getConfig()->setPointScaleFactor(0);
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
    artboard()->markLayoutDirty(this);
    markLayoutStyleDirty();
    if (parent() != nullptr && parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->syncLayoutChildren();
    }
    return StatusCode::Ok;
}

void LayoutComponent::update(ComponentDirt value)
{
    if (hasDirt(value, ComponentDirt::WorldTransform))
    {
        Mat2D parentWorld = parent()->is<WorldTransformComponent>()
                                ? (parent()->as<WorldTransformComponent>())->worldTransform()
                                : Mat2D();
        auto transform = Mat2D();
        transform[4] = m_layoutLocationX;
        transform[5] = m_layoutLocationY;

        auto multipliedTransform = Mat2D::multiply(parentWorld, transform);
        m_WorldTransform = multipliedTransform;
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
        if (child->is<TransformComponent>())
        {
            auto transformComponent = child->as<TransformComponent>();
            Vec2D measured =
                transformComponent->measureLayout(width, widthMode, height, heightMode);
            size = Vec2D(std::max(size.x, measured.x), std::max(size.y, measured.y));
        }
    }
    return size;
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
    if (m_style->widthUnits() != YGUnitAuto)
    {
        ygStyle.dimensions()[YGDimensionWidth] = YGValue{width(), m_style->widthUnits()};
    }
    else
    {
        ygStyle.dimensions()[YGDimensionWidth] = YGValueAuto;
    }
    if (m_style->heightUnits() != YGUnitAuto)
    {
        ygStyle.dimensions()[YGDimensionHeight] = YGValue{height(), m_style->heightUnits()};
    }
    else
    {
        ygStyle.dimensions()[YGDimensionHeight] = YGValueAuto;
    }

    bool isRowForAlignment = m_style->flexDirection() == YGFlexDirectionRow ||
                             m_style->flexDirection() == YGFlexDirectionRowReverse;
    switch (m_style->alignmentType())
    {
        case LayoutAlignmentType::topLeft:
        case LayoutAlignmentType::topCenter:
        case LayoutAlignmentType::topRight:
        case LayoutAlignmentType::spaceBetweenStart:
            if (isRowForAlignment)
            {
                ygStyle.alignItems() = YGAlignFlexStart;
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
    ygStyle.flexGrow() = YGFloatOptional(m_style->flexGrow());
    ygStyle.flexShrink() = YGFloatOptional(m_style->flexShrink());
    // ygStyle.flexBasis() = m_style->flexBasis();
    ygStyle.flexDirection() = m_style->flexDirection();
    ygStyle.flexWrap() = m_style->flexWrap();
    // ygStyle.alignItems() = m_style->alignItems();
    // ygStyle.alignContent() = m_style->alignContent();
    ygStyle.alignSelf() = m_style->alignSelf();
    // ygStyle.justifyContent() = m_style->justifyContent();

    ygNode.setStyle(ygStyle);
}

void LayoutComponent::syncLayoutChildren()
{
    YGNodeRemoveAllChildren(&layoutNode());
    int index = 0;
    for (size_t i = 0; i < children().size(); i++)
    {
        Component* child = children()[i];
        if (child->is<LayoutComponent>())
        {
            YGNodeInsertChild(&layoutNode(), &child->as<LayoutComponent>()->layoutNode(), index);
            index += 1;
        }
    }
}

void LayoutComponent::propagateSize()
{
    if (artboard() == this)
    {
        return;
    }
    propagateSizeToChildren(this);
}

void LayoutComponent::propagateSizeToChildren(ContainerComponent* component)
{
    for (auto child : component->children())
    {
        if (child->is<LayoutComponent>() || child->coreType() == NodeBase::typeKey)
        {
            continue;
        }
        if (child->is<TransformComponent>())
        {
            auto sizableChild = child->as<TransformComponent>();
            sizableChild->controlSize(Vec2D(m_layoutSizeWidth, m_layoutSizeHeight));
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

void LayoutComponent::updateLayoutBounds()
{
    auto node = &layoutNode();
    auto left = YGNodeLayoutGetLeft(node);
    auto top = YGNodeLayoutGetTop(node);
    auto width = YGNodeLayoutGetWidth(node);
    auto height = YGNodeLayoutGetHeight(node);
    if (animates())
    {
        auto toBounds = m_animationData.toBounds;
        if (left != toBounds.left() || top != toBounds.top() || width != toBounds.width() ||
            height != toBounds.height())
        {
            m_animationData.elapsedSeconds = 0;
            m_animationData.fromBounds = AABB(m_layoutLocationX,
                                              m_layoutLocationY,
                                              m_layoutLocationX + this->width(),
                                              m_layoutLocationY + this->height());
            m_animationData.toBounds = AABB(left, top, left + width, top + height);
            propagateSize();
            markWorldTransformDirty();
        }
    }
    else if (left != m_layoutLocationX || top != m_layoutLocationY || width != m_layoutSizeWidth ||
             height != m_layoutSizeHeight)
    {
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
        m_layoutSizeWidth = m_animationData.toBounds.width();
        m_layoutSizeHeight = m_animationData.toBounds.height();
        m_animationData.elapsedSeconds = 0;
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
    }
    m_animationData.elapsedSeconds = m_animationData.elapsedSeconds + (float)elapsedSeconds;
    if (needsAdvance)
    {
        propagateSize();
        markWorldTransformDirty();
        markLayoutNodeDirty();
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
void LayoutComponent::markLayoutNodeDirty() {}
void LayoutComponent::markLayoutStyleDirty() {}
#endif

void LayoutComponent::clipChanged() { markLayoutNodeDirty(); }
void LayoutComponent::widthChanged() { markLayoutNodeDirty(); }
void LayoutComponent::heightChanged() { markLayoutNodeDirty(); }
void LayoutComponent::styleIdChanged() { markLayoutNodeDirty(); }