#include "rive/nested_artboard_layout.hpp"
#include "rive/artboard.hpp"
#include "rive/math/aabb.hpp"
#include "rive/layout/layout_data.hpp"

using namespace rive;

Core* NestedArtboardLayout::clone() const
{
    NestedArtboardLayout* nestedArtboard =
        static_cast<NestedArtboardLayout*>(NestedArtboardLayoutBase::clone());
    nestedArtboard->file(file());
    if (m_Artboard == nullptr)
    {
        return nestedArtboard;
    }
    auto ni = m_Artboard->instance();
    nestedArtboard->nest(ni.release());
    return nestedArtboard;
}

AABB NestedArtboardLayout::layoutBounds()
{
#ifdef WITH_RIVE_LAYOUT
    if (artboardInstance() != nullptr)
    {
        return artboardInstance()->layoutBounds();
    }
#endif
    return AABB();
}

#ifdef WITH_RIVE_LAYOUT
void* NestedArtboardLayout::layoutNode(int index)
{
    if (artboardInstance() == nullptr)
    {
        return nullptr;
    }
    return static_cast<void*>(&artboardInstance()->takeLayoutData()->node);
}
#endif

void NestedArtboardLayout::markHostingLayoutDirty(
    ArtboardInstance* artboardInstance)
{
    if (artboard() != nullptr)
    {
        artboard()->markLayoutDirty(this->artboardInstance());
        artboard()->markLayoutStyleDirty();
    }
}

void NestedArtboardLayout::markLayoutNodeDirty(
    bool shouldForceUpdateLayoutBounds)
{
    updateWidthOverride();
    updateHeightOverride();
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
            auto correctedArtboardSpace = Mat2D::fromTranslation(
                parentArtboard->origin() + layoutPosition);
            m_WorldTransform = correctedArtboardSpace * m_WorldTransform;
        }
        else
        {
            m_WorldTransform =
                Mat2D::fromTranslation(layoutPosition) * m_WorldTransform;
        }
        auto back = Mat2D::fromTranslation(-artboard->origin());
        m_WorldTransform = back * m_WorldTransform;
    }
}

void NestedArtboardLayout::updateConstraints()
{
    if (m_layoutConstraints.size() > 0)
    {
        for (auto parentConstraint : m_layoutConstraints)
        {
            parentConstraint->constrainChild(this);
        }
    }
    Super::updateConstraints();
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

void NestedArtboardLayout::updateLayoutBounds(bool animate)
{
#ifdef WITH_RIVE_LAYOUT
    if (artboardInstance() == nullptr)
    {
        return;
    }
    artboardInstance()->updateLayoutBounds(animate);
#endif
}

void NestedArtboardLayout::updateWidthOverride()
{
    if (artboardInstance() == nullptr)
    {
        return;
    }
    m_styleOverrider.updateWidthOverride(artboardInstance());
}

void NestedArtboardLayout::updateHeightOverride()
{
    if (artboardInstance() == nullptr)
    {
        return;
    }
    m_styleOverrider.updateHeightOverride(artboardInstance());
}

bool NestedArtboardLayout::isRow()
{
    return parent()->is<LayoutComponent>()
               ? parent()->as<LayoutComponent>()->mainAxisIsRow()
               : true;
}

void NestedArtboardLayout::instanceWidthChanged() { updateWidthOverride(); }

void NestedArtboardLayout::instanceHeightChanged() { updateHeightOverride(); }

void NestedArtboardLayout::instanceWidthUnitsValueChanged()
{
    updateWidthOverride();
}

void NestedArtboardLayout::instanceHeightUnitsValueChanged()
{
    updateHeightOverride();
}

void NestedArtboardLayout::instanceWidthScaleTypeChanged()
{
    updateWidthOverride();
}

void NestedArtboardLayout::instanceHeightScaleTypeChanged()
{
    updateHeightOverride();
}

bool NestedArtboardLayout::syncStyleChanges()
{
    if (m_Artboard == nullptr)
    {
        return false;
    }
    return m_Artboard->syncStyleChanges();
}

void NestedArtboardLayout::updateArtboard(
    ViewModelInstanceArtboard* viewModelInstanceArtboard)
{
#ifdef WITH_RIVE_LAYOUT
    if (parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->clearLayoutChildren();
    }
#endif
    NestedArtboard::updateArtboard(viewModelInstanceArtboard);
    updateWidthOverride();
    updateHeightOverride();
#ifdef WITH_RIVE_LAYOUT
    if (parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->syncLayoutChildren();
    }
#endif
}