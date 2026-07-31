#ifndef _RIVE_GRID_ITEM_PLACEMENT_HPP_
#define _RIVE_GRID_ITEM_PLACEMENT_HPP_

#include "rive/generated/layout/grid_item_placement_base.hpp"
#include "rive/layout/layout_style_applier.hpp"

namespace rive
{
class ContainerComponent;

/// Explicit grid placement for one item. Absent means auto-placed with a span
/// of 1, which is the common case, so an auto-placed item stores nothing.
///
/// Applies itself: it writes the four grid line/span fields, which are pure
/// functions of its own properties. justifySelf is not here — it resolves
/// against the parent's justify-items and the item's hug state, which syncStyle
/// already has, so it stays on the style.
class GridItemPlacement : public GridItemPlacementBase,
                          public LayoutStyleApplier
{
public:
    /// The placement child of [owner], or nullptr. A walk rather than a cached
    /// pointer: the layout pass reaches placement through the applier list, so
    /// this is only for callers outside it (tooling, tests).
    static GridItemPlacement* from(const ContainerComponent* owner);

    StatusCode onAddedClean(CoreContext* context) override;
    void update(ComponentDirt value) override {}
    void buildDependencies() override;

#ifdef WITH_RIVE_LAYOUT
    void applyItemStyle(YGStyle& style,
                        const LayoutSyncContext& context) override;
#endif

protected:
    void gridColumnChanged() override;
    void gridRowChanged() override;
    void gridColumnSpanChanged() override;
    void gridRowSpanChanged() override;

private:
    void markOwnerDirty();
};
} // namespace rive

#endif
