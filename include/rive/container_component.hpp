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

    // Returns true if it searched through all of its children. predicate can
    // return false to stop searching.
    bool forAll(std::function<bool(Component*)> predicate);
    bool forEachChild(std::function<bool(Component*)> predicate);

private:
    std::vector<Component*> m_children;
};
} // namespace rive

#endif