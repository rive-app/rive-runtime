#ifndef _RIVE_LAYOUT_STYLE_APPLIER_HPP_
#define _RIVE_LAYOUT_STYLE_APPLIER_HPP_

#include <cstdint>

#ifdef WITH_RIVE_LAYOUT
// Yoga's type lives at global scope; declaring it inside namespace rive would
// silently make a different type.
class YGStyle;
#endif

namespace rive
{

/// Everything an applier needs to know about its surroundings, derived once per
/// sync by the owning layout item instead of re-derived by each applier. Values
/// that depend on the parent live here so an applier never has to walk upward.
struct LayoutSyncContext
{
    /// The parent lays its children out on a grid (grid or stack).
    bool parentIsGrid = false;
    /// The parent is a stack: every child collapses into the single cell, so
    /// per-item placement does not apply.
    bool parentIsStack = false;
    /// The parent's justify-items, which an item's justify-self inherits.
    uint32_t containerJustifyItems = 0;
    /// This item hugs on the inline axis and so cannot stretch.
    bool inlineHugs = false;
    /// This item fills on the inline axis. Resolved by the item's owner, since
    /// a hosted artboard is sized by its host's override rather than by the
    /// scale type stored on its own style.
    bool widthFills = false;
    /// The parent's main axis is a row. True when there is no layout parent,
    /// matching LayoutComponent::effectiveParentIsRow.
    bool parentIsRow = true;
    /// Resolved writing direction — inherited, so not readable off the style.
    bool isLTR = true;
    /// Without one, margin percents have nothing to resolve against and are
    /// treated as points.
    bool hasLayoutParent = false;
};

/// Implemented by any Core object that contributes to a layout item's yoga
/// style. Implementors register with the owning provider at onAddedClean and
/// are applied from syncStyle, replacing the single monolithic sync function.
///
/// One object may contribute to several phases, so the phase is the method and
/// ordering lives in LayoutData::applyLayoutStyles. Override what you
/// contribute.
///
/// An applier's *existence* tracks authored intent; whether it *applies* is
/// decided per-sync from the context. Nothing conditional on the parent's
/// current type should be created or destroyed — reparenting must not churn
/// objects or lose authored values.
class LayoutStyleApplier
{
public:
    virtual ~LayoutStyleApplier() {}
#ifdef WITH_RIVE_LAYOUT
    /// Sizing and the box model: this item's own width/height/margin/padding.
    virtual void applyBaseStyle(YGStyle& style,
                                const LayoutSyncContext& context)
    {}
    /// Container-level arrangement imposed on children: tracks, alignment.
    virtual void applyContainerStyle(YGStyle& style,
                                     const LayoutSyncContext& context)
    {}
    /// How this item sits inside its parent's arrangement.
    virtual void applyItemStyle(YGStyle& style,
                                const LayoutSyncContext& context)
    {}
    /// Explicit grid placement, applied after applyItemStyle.
    ///
    /// Its own phase because the item phase *resets* the grid cell
    /// (LayoutSizingStyle::applyItemStyle) — sharing a phase would make the
    /// winner depend on applier registration order, which is sibling order,
    /// which the file decides.
    virtual void applyPlacementStyle(YGStyle& style,
                                     const LayoutSyncContext& context)
    {}
#endif
};
} // namespace rive

#endif
