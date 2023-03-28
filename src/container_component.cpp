#include "rive/container_component.hpp"
using namespace rive;

void ContainerComponent::addChild(Component* component) { m_children.push_back(component); }

bool ContainerComponent::collapse(bool value)
{
    if (!Super::collapse(value))
    {
        return false;
    }
    for (Component* child : m_children)
    {
        child->collapse(value);
    }
    return true;
}