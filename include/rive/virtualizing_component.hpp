#ifndef _RIVE_VIRTUALIZING_COMPONENT_HPP_
#define _RIVE_VIRTUALIZING_COMPONENT_HPP_
#include "rive/math/vec2d.hpp"
#include <stdio.h>
#include <vector>
namespace rive
{
class LayoutNodeProvider;

enum class VirtualizedDirection
{
    horizontal,
    vertical
};

class Virtualizable
{
public:
    virtual Component* virtualizableComponent() = 0;
};

class VirtualizingComponent
{
public:
    static VirtualizingComponent* from(Component* component);
    // The list a scroll child is, or null.
    static VirtualizingComponent* fromScrollChild(LayoutNodeProvider* child);
    virtual bool virtualizationEnabled() = 0;
    virtual int itemCount() = 0;
    virtual Virtualizable* item(int index) = 0;
    virtual Vec2D size() = 0;
    virtual Vec2D itemSize(int index) = 0;
    virtual void setItemSize(Vec2D size, int index) = 0;
    virtual void addVirtualizable(int index) = 0;
    virtual void virtualizableChanged() = 0;
    virtual void removeVirtualizable(int index) = 0;
    // Appends the indices of the items it holds realized, wherever a list
    // update has moved them since they were added.
    virtual void realizedIndices(std::vector<int>& out) = 0;
    // Starts a new window: every item leaves it.
    virtual void clearVirtualWindow() = 0;
    // Adds a realized item to the window, in the order the virtualizer walks
    // them; realized items are drawn. Only visible (on screen) ones report
    // their measured size back, otherwise realizing an off screen item would
    // change the sizes the virtualizer used to pick this very window.
    virtual void addToVirtualWindow(int index, bool visible) = 0;
    virtual void setVirtualizablePosition(int index, Vec2D position) = 0;
    // Pins an item to a grid cell (0-based) so layout sizes it there; -1
    // leaves it to auto placement.
    virtual void setVirtualizableCell(int index, int column, int row) = 0;
};
} // namespace rive

#endif