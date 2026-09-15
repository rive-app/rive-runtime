#ifndef _RIVE_LAYOUT_SIZING_STYLE_HPP_
#define _RIVE_LAYOUT_SIZING_STYLE_HPP_
#include "rive/generated/layout/layout_sizing_style_base.hpp"
#include "rive/layout/layout_style_applier.hpp"
#ifdef WITH_RIVE_LAYOUT
#include "yoga/Yoga.h"
#endif
namespace rive
{
/// Sizing shared by both laid-out shapes: a LayoutComponent (via
/// LayoutComponentStyle) and a participant (via LayoutNodeStyle). Anything
/// derived only from these properties belongs here, so both paths share it.
class LayoutSizingStyle : public LayoutSizingStyleBase,
                          public LayoutStyleApplier
{
public:
#ifdef WITH_RIVE_LAYOUT
    /// A participant is a leaf, so only hidden or flex. LayoutComponentStyle
    /// overrides to also select grid, which needs its layoutType.
    virtual YGDisplay display();

    YGUnit maxWidthUnits();
    YGUnit maxHeightUnits();
    YGUnit minWidthUnits();
    YGUnit minHeightUnits();

    void applyBaseStyle(YGStyle& style,
                        const LayoutSyncContext& context) override;
    void applyItemStyle(YGStyle& style,
                        const LayoutSyncContext& context) override;
#endif
};
} // namespace rive

#endif
