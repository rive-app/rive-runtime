#ifndef _RIVE_CONTAINER_COMPONENT_HPP_
#define _RIVE_CONTAINER_COMPONENT_HPP_
#include "rive/generated/container_component_base.hpp"
#include <vector>
#include <functional>

namespace rive
{
class ContainerComponent : public ContainerComponentBase
{
public:
    const std::vector<Component*>& children() const { return m_children; }
    virtual void addChild(Component* component);
    bool collapse(bool value) override;

    // Returns whether predicate returns true for the current Component.
    bool forAll(std::function<bool(Component*)> predicate);

    // Recursively descend onto all the children in the hierarchy tree.
    // If predicate returns false, it won't recurse down a particular branch.
    void forEachChild(std::function<bool(Component*)> predicate);

private:
    std::vector<Component*> m_children;
};
} // namespace rive

#endif