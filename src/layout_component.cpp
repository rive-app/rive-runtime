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

AABB LayoutComponent::findMaxIntrinsicSize(ContainerComponent* component, AABB maxIntrinsicSize)
{
    auto intrinsicSize = maxIntrinsicSize;
    for (auto child : component->children())
    {
        if (child->is<LayoutComponent>())
        {
            continue;
        }
        if (child->is<TransformComponent>())
        {
            auto sizableChild = child->as<TransformComponent>();
            auto minSize =
                AABB::fromLTWH(0,
                               0,
                               style()->minWidthUnits() == YGUnitPoint ? style()->minWidth() : 0,
                               style()->minHeightUnits() == YGUnitPoint ? style()->minHeight() : 0);
            auto maxSize = AABB::fromLTWH(
                0,
                0,
                style()->maxWidthUnits() == YGUnitPoint ? style()->maxWidth()
                                                        : std::numeric_limits<float>::infinity(),
                style()->maxHeightUnits() == YGUnitPoint ? style()->maxHeight()
                                                         : std::numeric_limits<float>::infinity());
            auto size = sizableChild->computeIntrinsicSize(minSize, maxSize);
            intrinsicSize = AABB::fromLTWH(0,
                                           0,
                                           std::max(maxIntrinsicSize.width(), size.width()),
                                           std::max(maxIntrinsicSize.height(), size.height()));
        }
        if (child->is<ContainerComponent>())
        {
            return findMaxIntrinsicSize(child->as<ContainerComponent>(), intrinsicSize);
        }
    }
    return intrinsicSize;
}

void LayoutComponent::syncStyle()
{
    if (style() == nullptr || layoutStyle() == nullptr || layoutNode() == nullptr)
    {
        return;
    }
    bool setIntrinsicWidth = false;
    bool setIntrinsicHeight = false;
    if (style()->intrinsicallySized() &&
        (style()->widthUnits() == YGUnitAuto || style()->heightUnits() == YGUnitAuto))
    {
        AABB intrinsicSize = findMaxIntrinsicSize(this, AABB());
        bool foundIntrinsicSize = intrinsicSize.width() != 0 || intrinsicSize.height() != 0;

        if (foundIntrinsicSize)
        {
            if (style()->widthUnits() == YGUnitAuto)
            {
                setIntrinsicWidth = true;
                layoutStyle()->dimensions()[YGDimensionWidth] =
                    YGValue{intrinsicSize.width(), YGUnitPoint};
            }
            if (style()->heightUnits() == YGUnitAuto)
            {
                setIntrinsicHeight = true;
                layoutStyle()->dimensions()[YGDimensionHeight] =
                    YGValue{intrinsicSize.height(), YGUnitPoint};
            }
        }
    }
    if (!setIntrinsicWidth)
    {
        layoutStyle()->dimensions()[YGDimensionWidth] = YGValue{width(), style()->widthUnits()};
    }
    if (!setIntrinsicHeight)
    {
        layoutStyle()->dimensions()[YGDimensionHeight] = YGValue{height(), style()->heightUnits()};
    }
    layoutStyle()->minDimensions()[YGDimensionWidth] =
        YGValue{style()->minWidth(), style()->minWidthUnits()};
    layoutStyle()->minDimensions()[YGDimensionHeight] =
        YGValue{style()->minHeight(), style()->minHeightUnits()};
    layoutStyle()->maxDimensions()[YGDimensionWidth] =
        YGValue{style()->maxWidth(), style()->maxWidthUnits()};
    layoutStyle()->maxDimensions()[YGDimensionHeight] =
        YGValue{style()->maxHeight(), style()->maxHeightUnits()};

    layoutStyle()->gap()[YGGutterColumn] =
        YGValue{style()->gapHorizontal(), style()->gapHorizontalUnits()};
    layoutStyle()->gap()[YGGutterRow] =
        YGValue{style()->gapVertical(), style()->gapVerticalUnits()};
    layoutStyle()->border()[YGEdgeLeft] =
        YGValue{style()->borderLeft(), style()->borderLeftUnits()};
    layoutStyle()->border()[YGEdgeRight] =
        YGValue{style()->borderRight(), style()->borderRightUnits()};
    layoutStyle()->border()[YGEdgeTop] = YGValue{style()->borderTop(), style()->borderTopUnits()};
    layoutStyle()->border()[YGEdgeBottom] =
        YGValue{style()->borderBottom(), style()->borderBottomUnits()};
    layoutStyle()->margin()[YGEdgeLeft] =
        YGValue{style()->marginLeft(), style()->marginLeftUnits()};
    layoutStyle()->margin()[YGEdgeRight] =
        YGValue{style()->marginRight(), style()->marginRightUnits()};
    layoutStyle()->margin()[YGEdgeTop] = YGValue{style()->marginTop(), style()->marginTopUnits()};
    layoutStyle()->margin()[YGEdgeBottom] =
        YGValue{style()->marginBottom(), style()->marginBottomUnits()};
    layoutStyle()->padding()[YGEdgeLeft] =
        YGValue{style()->paddingLeft(), style()->paddingLeftUnits()};
    layoutStyle()->padding()[YGEdgeRight] =
        YGValue{style()->paddingRight(), style()->paddingRightUnits()};
    layoutStyle()->padding()[YGEdgeTop] =
        YGValue{style()->paddingTop(), style()->paddingTopUnits()};
    layoutStyle()->padding()[YGEdgeBottom] =
        YGValue{style()->paddingBottom(), style()->paddingBottomUnits()};
    layoutStyle()->position()[YGEdgeLeft] =
        YGValue{style()->positionLeft(), style()->positionLeftUnits()};
    layoutStyle()->position()[YGEdgeRight] =
        YGValue{style()->positionRight(), style()->positionRightUnits()};
    layoutStyle()->position()[YGEdgeTop] =
        YGValue{style()->positionTop(), style()->positionTopUnits()};
    layoutStyle()->position()[YGEdgeBottom] =
        YGValue{style()->positionBottom(), style()->positionBottomUnits()};

    layoutStyle()->display() = style()->display();
    layoutStyle()->positionType() = style()->positionType();
    layoutStyle()->flex() = YGFloatOptional(style()->flex());
    layoutStyle()->flexGrow() = YGFloatOptional(style()->flexGrow());
    layoutStyle()->flexShrink() = YGFloatOptional(style()->flexShrink());
    // layoutStyle()->flexBasis() = style()->flexBasis();
    layoutStyle()->flexDirection() = style()->flexDirection();
    layoutStyle()->flexWrap() = style()->flexWrap();
    layoutStyle()->alignItems() = style()->alignItems();
    layoutStyle()->alignContent() = style()->alignContent();
    layoutStyle()->alignSelf() = style()->alignSelf();
    layoutStyle()->justifyContent() = style()->justifyContent();

    layoutNode()->setStyle(*layoutStyle());
}

void LayoutComponent::syncLayoutChildren()
{
    YGNodeRemoveAllChildren(layoutNode());
    int index = 0;
    for (size_t i = 0; i < children().size(); i++)
    {
        Component* child = children()[i];
        if (child->is<LayoutComponent>())
        {
            YGNodeInsertChild(layoutNode(), child->as<LayoutComponent>()->layoutNode(), index);
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
            sizableChild->controlSize(AABB::fromLTWH(0, 0, m_layoutSizeWidth, m_layoutSizeHeight));
        }
        if (child->is<ContainerComponent>())
        {
            propagateSizeToChildren(child->as<ContainerComponent>());
        }
    }
}

void LayoutComponent::calculateLayout()
{
    YGNodeCalculateLayout(layoutNode(), width(), height(), YGDirection::YGDirectionInherit);
}

void LayoutComponent::updateLayoutBounds()
{
    auto left = YGNodeLayoutGetLeft(layoutNode());
    auto top = YGNodeLayoutGetTop(layoutNode());
    auto width = YGNodeLayoutGetWidth(layoutNode());
    auto height = YGNodeLayoutGetHeight(layoutNode());
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
    layoutNode()->markDirtyAndPropagate();
    artboard()->markLayoutDirty(this);
}
#else
void LayoutComponent::markLayoutNodeDirty() {}
#endif

void LayoutComponent::clipChanged() { markLayoutNodeDirty(); }
void LayoutComponent::widthChanged() { markLayoutNodeDirty(); }
void LayoutComponent::heightChanged() { markLayoutNodeDirty(); }
void LayoutComponent::styleIdChanged() { markLayoutNodeDirty(); }