#ifndef _RIVE_CONTAINER_COMPONENT_HPP_
#define _RIVE_CONTAINER_COMPONENT_HPP_
#include "rive/generated/container_component_base.hpp"
#include <vector>
namespace rive
{
class ContainerComponent : public ContainerComponentBase
{
private:
    std::vector<Component*> m_children;

public:
    const std::vector<Component*>& children() const { return m_children; }
    virtual void addChild(Component* component);
    bool collapse(bool value) override;
};
} // namespace rive

#endif