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

bool ContainerComponent::forAll(std::function<bool(Component*)> predicate)
{
    if (!predicate(this))
    {
        return false;
    }
    forEachChild(predicate);
    return true;
}

bool ContainerComponent::forEachChild(std::function<bool(Component*)> predicate)
{
    for (Component* child : m_children)
    {
        if (!predicate(child))
        {
            return false;
        }
        if (child->is<ContainerComponent>() &&
            !child->as<ContainerComponent>()->forEachChild(predicate))
        {
            return false;
        }
    }
    return true;
}