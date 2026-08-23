#include "rive/nested_artboard_layout.hpp"
#include "rive/artboard.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/layout/layout_data.hpp"
#include "rive/math/aabb.hpp"

using namespace rive;

Core* NestedArtboardLayout::clone() const
{
    NestedArtboardLayout* nestedArtboard =
        static_cast<NestedArtboardLayout*>(NestedArtboardLayoutBase::clone());
    nestedArtboard->file(file());
    if (m_referencedArtboard == nullptr)
    {
        return nestedArtboard;
    }
    auto ni = m_referencedArtboard->instance();
    nestedArtboard->referencedArtboard(ni.release());
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

// Where the layout put us, less the mounted artboard's own origin, as one base
// translation — the same shape Shape::layoutBaseTranslation builds for a
// participant:
//
//   parentWorld * translate(slot - origin) * m_Transform
//
// Done here rather than after Super::update so the base's
// composeWorldTransform/updateConstraints pair stays in order: constraints run
// against the composed transform, and their result is not overwritten by a
// later recomposition. Same split LayoutComponent uses.
void NestedArtboardLayout::composeWorldTransform()
{
    auto artboard = artboardInstance();
    if (artboard == nullptr)
    {
        Super::composeWorldTransform();
        return;
    }
    auto base =
        Vec2D(artboard->layoutX(), artboard->layoutY()) - artboard->origin();
    if (parent()->is<Artboard>())
    {
        base += parent()->as<Artboard>()->origin();
    }
    Mat2D parentWorld =
        parent()->is<WorldTransformComponent>()
            ? parent()->as<WorldTransformComponent>()->worldTransform()
            : Mat2D();
    m_WorldTransform = parentWorld * Mat2D::fromTranslation(base) * m_Transform;
}

void NestedArtboardLayout::updateConstraints()
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

#ifdef WITH_RIVE_LAYOUT
bool NestedArtboardLayout::cascadeLayoutStyle(
    LayoutStyleInterpolation inheritedInterpolation,
    KeyFrameInterpolator* inheritedInterpolator,
    float inheritedInterpolationTime,
    LayoutDirection direction)
{
    if (artboardInstance() != nullptr)
    {
        artboardInstance()->cascadeLayoutStyle(inheritedInterpolation,
                                               inheritedInterpolator,
                                               inheritedInterpolationTime,
                                               direction);
    }
    return false;
}
#endif

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

// The layout that collected us, which may sit above a container. Asking
// parent() reported row/not-stack for a Solo and sized the wrong axis.
bool NestedArtboardLayout::isRow()
{
    auto* layout = owningLayout(parent());
    return layout != nullptr ? layout->mainAxisIsRow() : true;
}

bool NestedArtboardLayout::isStack()
{
    auto* layout = owningLayout(parent());
    return layout != nullptr && layout->isStackContainer();
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
    if (m_referencedArtboard == nullptr)
    {
        return false;
    }
    return m_referencedArtboard->syncStyleChanges();
}

void NestedArtboardLayout::updateArtboard(
    ViewModelInstanceArtboard* viewModelInstanceArtboard)
{
#ifdef WITH_RIVE_LAYOUT
    // Re-collect on the layout that owns our node, not on parent(), or a
    // container between the two leaves the swap unsynced.
    auto* layout = owningLayout(parent());
    if (layout != nullptr)
    {
        layout->clearLayoutChildren();
    }
#endif
    NestedArtboard::updateArtboard(viewModelInstanceArtboard);
    updateWidthOverride();
    updateHeightOverride();
#ifdef WITH_RIVE_LAYOUT
    if (layout != nullptr)
    {
        layout->syncLayoutChildren();
    }
#endif
}