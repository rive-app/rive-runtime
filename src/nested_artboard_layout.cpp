#include "rive/nested_artboard_layout.hpp"
#include "rive/artboard.hpp"

using namespace rive;

Core* NestedArtboardLayout::clone() const
{
    NestedArtboardLayout* nestedArtboard =
        static_cast<NestedArtboardLayout*>(NestedArtboardLayoutBase::clone());
    if (m_Artboard == nullptr)
    {
        return nestedArtboard;
    }
    auto ni = m_Artboard->instance();
    nestedArtboard->nest(ni.release());
    return nestedArtboard;
}

float NestedArtboardLayout::actualInstanceWidth()
{
    return instanceWidth() == -1.0f ? artboardInstance()->originalWidth() : instanceWidth();
}

float NestedArtboardLayout::actualInstanceHeight()
{
    return instanceHeight() == -1.0f ? artboardInstance()->originalHeight() : instanceHeight();
}

#ifdef WITH_RIVE_LAYOUT
void* NestedArtboardLayout::layoutNode()
{
    if (artboardInstance() == nullptr)
    {
        return nullptr;
    }
    return artboardInstance()->takeLayoutNode();
}
#endif

void NestedArtboardLayout::markNestedLayoutDirty()
{
    if (artboardInstance() != nullptr)
    {
        artboardInstance()->markLayoutNodeDirty();
    }
}

void NestedArtboardLayout::update(ComponentDirt value)
{
    Super::update(value);
    auto artboard = artboardInstance();
    if (hasDirt(value, ComponentDirt::WorldTransform) && artboard != nullptr)
    {
        auto layoutPosition = Vec2D(artboard->layoutX(), artboard->layoutY());

        if (parent()->is<Artboard>())
        {
            auto parentArtboard = parent()->as<Artboard>();
            auto correctedArtboardSpace =
                Mat2D::fromTranslation(parentArtboard->origin() + layoutPosition);
            m_WorldTransform = correctedArtboardSpace * m_WorldTransform;
        }
        else
        {
            m_WorldTransform = Mat2D::fromTranslation(layoutPosition) * m_WorldTransform;
        }
        auto back = Mat2D::fromTranslation(-artboard->origin());
        m_WorldTransform = back * m_WorldTransform;
    }
}

StatusCode NestedArtboardLayout::onAddedClean(CoreContext* context)
{
    StatusCode code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    updateWidthOverride();
    updateHeightOverride();

    return StatusCode::Ok;
}

void NestedArtboardLayout::updateWidthOverride()
{
    if (artboardInstance() == nullptr)
    {
        return;
    }
    auto isRow =
        parent()->is<LayoutComponent>() ? parent()->as<LayoutComponent>()->mainAxisIsRow() : true;
    if (instanceWidthScaleType() == 0) // LayoutScaleType::fixed
    {
        // If we're set to fixed, pass the unit value (points|percent)
        artboardInstance()->widthOverride(actualInstanceWidth(), instanceWidthUnitsValue(), isRow);
    }
    else if (instanceWidthScaleType() == 1) // LayoutScaleType::fill
    {
        // If we're set to fill, pass auto
        artboardInstance()->widthOverride(actualInstanceWidth(), 3, isRow);
    }
}

void NestedArtboardLayout::updateHeightOverride()
{
    if (artboardInstance() == nullptr)
    {
        return;
    }
    auto isRow =
        parent()->is<LayoutComponent>() ? parent()->as<LayoutComponent>()->mainAxisIsRow() : true;
    if (instanceHeightScaleType() == 0) // LayoutScaleType::fixed
    {
        // If we're set to fixed, pass the unit value (points|percent)
        artboardInstance()->heightOverride(actualInstanceHeight(),
                                           instanceHeightUnitsValue(),
                                           isRow);
    }
    else if (instanceHeightScaleType() == 1) // LayoutScaleType::fill
    {
        // If we're set to fill, pass auto
        artboardInstance()->heightOverride(actualInstanceHeight(), 3, isRow);
    }
}

void NestedArtboardLayout::instanceWidthChanged() { updateWidthOverride(); }

void NestedArtboardLayout::instanceHeightChanged() { updateHeightOverride(); }

void NestedArtboardLayout::instanceWidthUnitsValueChanged() { updateWidthOverride(); }

void NestedArtboardLayout::instanceHeightUnitsValueChanged() { updateHeightOverride(); }

void NestedArtboardLayout::instanceWidthScaleTypeChanged() { updateWidthOverride(); }

void NestedArtboardLayout::instanceHeightScaleTypeChanged() { updateHeightOverride(); }