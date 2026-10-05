#ifndef _RIVE_SCROLL_VIRTUALIZER_HPP_
#define _RIVE_SCROLL_VIRTUALIZER_HPP_
#include "rive/artboard.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/world_transform_component.hpp"
#include "rive/virtualizing_component.hpp"
#include <stdio.h>
#include <vector>
namespace rive
{
class LayoutNodeProvider;
class ScrollConstraint;
class VirtualLayout;

// Realizes the items of the scroll's VirtualLayout that the viewport reaches,
// recycling the rest. Owns no geometry.
class ScrollVirtualizer
{
private:
    // First item on screen after the last pass, where its line started, and
    // how many items there were; -1 when there's nothing to anchor.
    int m_anchorItem = -1;
    float m_anchorStart = 0;
    int m_anchorItemCount = 0;
    // The anchor's realized instance, which a list update moves along with
    // its item; null when the anchor isn't a list item.
    Virtualizable* m_anchorInstance = nullptr;
    float m_offset = 0;
    bool m_infinite = false;
    float m_viewportSize = 0;
    // Scrolled both ways, the offset along the lines. A grid windows columns
    // over this span of its content.
    bool m_windowsColumns = false;
    // Whether the last pass pinned grid cells, which the next must undo.
    bool m_pinnedCells = false;
    float m_flowOffset = 0;
    float m_columnStart = 0;
    float m_columnViewport = 0;
    VirtualizedDirection m_direction = VirtualizedDirection::horizontal;

public:
    ~ScrollVirtualizer();
    void reset();
    bool constrain(ScrollConstraint* scroll,
                   std::vector<LayoutNodeProvider*>& children,
                   float offset,
                   VirtualizedDirection direction,
                   float flowOffset = 0.0f,
                   float columnStart = 0.0f,
                   float columnViewport = 0.0f,
                   bool windowsColumns = false);
    void virtualize(ScrollConstraint* scroll,
                    std::vector<LayoutNodeProvider*>& children);
    // How far the anchor item's line moved along the scroll axis since the
    // last pass, given the rebuilt layout; 0 when there's no anchor.
    float anchorMoved(const VirtualLayout& layout,
                      std::vector<LayoutNodeProvider*>& children) const;
};
} // namespace rive

#endif
