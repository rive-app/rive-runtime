#include "rive/layout/grid_item_placement.hpp"
#include "rive/container_component.hpp"
#include "rive/layout/grid_track.hpp"
#include "rive/layout/layout_node_provider.hpp"

using namespace rive;

GridItemPlacement* GridItemPlacement::from(const ContainerComponent* owner)
{
    if (owner == nullptr)
    {
        return nullptr;
    }
    for (auto* child : owner->children())
    {
        if (child->is<GridItemPlacement>())
        {
            return child->as<GridItemPlacement>();
        }
    }
    return nullptr;
}

StatusCode GridItemPlacement::onAddedClean(CoreContext* context)
{
    StatusCode code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
#ifdef WITH_RIVE_LAYOUT
    // from() already resolves both shapes: a LayoutComponent provides for
    // itself, while Text/Image/Shape provide via their LayoutParticipant child.
    // onAddedClean runs after every object has resolved its parent, so the
    // sibling provider is findable now.
    if (auto* provider = LayoutNodeProvider::from(parent()))
    {
        provider->addLayoutStyleApplier(this);
    }
#endif
    return StatusCode::Ok;
}

void GridItemPlacement::buildDependencies()
{
    Super::buildDependencies();
    if (parent() != nullptr)
    {
        parent()->addDependent(this);
    }
}

#ifdef WITH_RIVE_LAYOUT
void GridItemPlacement::applyPlacementStyle(YGStyle& style,
                                            const LayoutSyncContext& context)
{
    // A stack collapses every child into its single cell, so explicit placement
    // has nothing to say. Note this is a no-op rather than an absent applier:
    // reparenting between a grid and a stack must not create or destroy the
    // object, or the authored placement would be lost on the round trip.
    if (context.parentIsStack || !context.parentIsGrid)
    {
        return;
    }
    GridTrack::syncItemLines(style,
                             gridColumn(),
                             gridRow(),
                             gridColumnSpan(),
                             gridRowSpan());
}
#endif

void GridItemPlacement::markOwnerDirty()
{
    if (auto* provider = LayoutNodeProvider::from(parent()))
    {
        provider->markLayoutNodeDirty();
    }
}

void GridItemPlacement::gridColumnChanged() { markOwnerDirty(); }
void GridItemPlacement::gridRowChanged() { markOwnerDirty(); }
void GridItemPlacement::gridColumnSpanChanged() { markOwnerDirty(); }
void GridItemPlacement::gridRowSpanChanged() { markOwnerDirty(); }
