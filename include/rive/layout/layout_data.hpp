#ifndef _RIVE_LAYOUT_DATA_HPP_
#define _RIVE_LAYOUT_DATA_HPP_

#ifdef WITH_RIVE_LAYOUT
#include "rive/layout/layout_style_applier.hpp"
#include "rive/lazy_vector.hpp"
#include "yoga/YGNode.h"
#include "yoga/YGStyle.h"
#include "yoga/Yoga.h"
#endif
#ifdef WITH_RIVE_TOOLS
#include "rive/refcnt.hpp"
#include <unordered_set>
#endif

namespace rive
{
class LayoutData
#ifdef WITH_RIVE_TOOLS
    : public RefCnt<LayoutData>
#endif
{
public:
#ifdef WITH_RIVE_LAYOUT

#ifdef WITH_RIVE_TOOLS
    std::unordered_set<LayoutData*> children;
#ifdef DEBUG
    LayoutData() { count++; }
#endif
    ~LayoutData()
    {
#ifdef DEBUG
        count--;
#endif
        clearChildren();
    }
    void clearChildren()
    {
        for (auto child : children)
        {
            child->unref();
        }
        children.clear();
    }
#ifdef DEBUG
    static uint32_t count;
#endif
#endif

    YGNode node;
    YGStyle style;

    /// Objects contributing to this item's style. Lazy, so an item with none
    /// pays one null pointer. Unsorted — apply order comes from the phase
    /// methods, not from list order, which follows file order.
    LazyVector<LayoutStyleApplier*> appliers;

    /// No matching remove: runtime objects live for the artboard's lifetime.
    void addApplier(LayoutStyleApplier* applier)
    {
        appliers.pushUnique(applier);
    }

    /// Runs every applier phase by phase, so apply order can't depend on child
    /// order.
    void applyLayoutStyles(YGStyle& style, const LayoutSyncContext& context)
    {
        if (appliers.empty())
        {
            return;
        }
        for (auto* applier : appliers)
        {
            applier->applyBaseStyle(style, context);
        }
        for (auto* applier : appliers)
        {
            applier->applyContainerStyle(style, context);
        }
        for (auto* applier : appliers)
        {
            applier->applyItemStyle(style, context);
        }
    }
#endif
};

#ifdef WITH_RIVE_TOOLS
typedef rcp<LayoutData> LayoutDataRef;
#else
typedef LayoutData* LayoutDataRef;
#endif

} // namespace rive
#endif