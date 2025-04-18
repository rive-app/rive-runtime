#ifndef _RIVE_LAYOUT_NODE_PROVIDER_HPP_
#define _RIVE_LAYOUT_NODE_PROVIDER_HPP_

#include <assert.h>
#include <vector>

namespace rive
{
class AABB;
class Component;
class LayoutConstraint;
class TransformComponent;

class LayoutNodeProvider
{
protected:
    std::vector<LayoutConstraint*> m_layoutConstraints;

public:
#ifdef WITH_RIVE_LAYOUT
    virtual void* layoutNode(int index) = 0;
#endif
    static LayoutNodeProvider* from(Component* component);
    virtual TransformComponent* transformComponent() = 0;
    void addLayoutConstraint(LayoutConstraint* constraint);
    virtual AABB layoutBounds() = 0;
    virtual bool syncStyleChanges() { return false; };
    virtual void updateLayoutBounds(bool animate = true){};
    virtual void markLayoutNodeDirty(bool shouldForceUpdateLayoutBounds = false)
    {}
};
} // namespace rive

#endif