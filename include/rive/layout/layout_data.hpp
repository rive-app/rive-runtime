#ifndef _RIVE_LAYOUT_DATA_HPP_
#define _RIVE_LAYOUT_DATA_HPP_

#ifdef WITH_RIVE_LAYOUT
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
#endif
};

#ifdef WITH_RIVE_TOOLS
typedef rcp<LayoutData> LayoutDataRef;
#else
typedef LayoutData* LayoutDataRef;
#endif

} // namespace rive
#endif