#ifndef _RIVE_VIRTUALIZING_COMPONENT_HPP_
#define _RIVE_VIRTUALIZING_COMPONENT_HPP_
#include "rive/math/vec2d.hpp"
#include <stdio.h>
namespace rive
{

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
    virtual bool virtualizationEnabled() = 0;
    virtual int itemCount() = 0;
    virtual Virtualizable* item(int index) = 0;
    virtual Vec2D size() = 0;
    virtual Vec2D itemSize(int index) = 0;
    virtual void setItemSize(Vec2D size, int index) = 0;
    virtual void addVirtualizable(int index) = 0;
    virtual void removeVirtualizable(int index) = 0;
    virtual void setVisibleIndices(int start, int end) = 0;
    virtual void setVirtualizablePosition(int index, Vec2D position) = 0;
};
} // namespace rive

#endif