#ifndef _RIVE_SCROLL_VIRTUALIZER_HPP_
#define _RIVE_SCROLL_VIRTUALIZER_HPP_
#include "rive/artboard.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/world_transform_component.hpp"
#include "rive/virtualizing_component.hpp"
#include <stdio.h>
#include <unordered_map>
namespace rive
{
class LayoutNodeProvider;
class ScrollConstraint;

class ScrollVirtualizer
{
private:
    int m_realizedIndexStart = 0;
    int m_realizedIndexEnd = 0;
    float m_offset = 0;
    bool m_infinite = false;
    float m_viewportSize = 0;
    VirtualizedDirection m_direction = VirtualizedDirection::horizontal;

    void recycleItems(std::vector<int> indices,
                      std::vector<LayoutNodeProvider*>& children,
                      int totalItemCount);
    float getItemSize(LayoutNodeProvider* child, int index, bool isHorizontal);
    // Size of a global (list-wide) index, resolved across child providers.
    float getItemSizeAt(int globalIndex,
                        std::vector<LayoutNodeProvider*>& children,
                        int totalItemCount,
                        bool isHorizontal);

public:
    ~ScrollVirtualizer();
    void reset();
    bool constrain(ScrollConstraint* scroll,
                   std::vector<LayoutNodeProvider*>& children,
                   float offset,
                   VirtualizedDirection direction);
    void virtualize(ScrollConstraint* scroll,
                    std::vector<LayoutNodeProvider*>& children);
};
} // namespace rive

#endif