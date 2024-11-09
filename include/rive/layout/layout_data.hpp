#ifndef _RIVE_LAYOUT_DATA_HPP_
#define _RIVE_LAYOUT_DATA_HPP_

#ifdef WITH_RIVE_LAYOUT
#include "yoga/YGNode.h"
#include "yoga/YGStyle.h"
#include "yoga/Yoga.h"
#endif

namespace rive
{
struct LayoutData
{
#ifdef WITH_RIVE_LAYOUT
    YGNode node;
    YGStyle style;
#endif
};

} // namespace rive
#endif