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
LayoutComponent::LayoutComponent() : m_layoutData(std::unique_ptr<LayoutData>(new LayoutData())) {}

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
    ygStyle.alignItems() = m_style->alignItems();
    ygStyle.alignContent() = m_style->alignContent();
    ygStyle.alignSelf() = m_style->alignSelf();
    ygStyle.justifyContent() = m_style->justifyContent();

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
    if (left != m_layoutLocationX || top != m_layoutLocationY || width != m_layoutSizeWidth ||
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

void LayoutComponent::markLayoutNodeDirty()
{
    layoutNode().markDirtyAndPropagate();
    artboard()->markLayoutDirty(this);
}
#else
void LayoutComponent::markLayoutNodeDirty() {}
#endif

void LayoutComponent::clipChanged() { markLayoutNodeDirty(); }
void LayoutComponent::widthChanged() { markLayoutNodeDirty(); }
void LayoutComponent::heightChanged() { markLayoutNodeDirty(); }
void LayoutComponent::styleIdChanged() { markLayoutNodeDirty(); }