#include "rive/container_component.hpp"
#include <algorithm>
using namespace rive;

void ContainerComponent::addChild(Component* component)
{
#ifdef WITH_RIVE_EDITOR
    // Editor build: dedupe. Journal replay re-fires `parentIdChanged`
    // (which calls addChild via the core_hook callback) AND walks the
    // recreated Core through `onAddedDirty` (which also calls addChild
    // in `Component::onAddedDirty`'s editor branch). Without dedupe
    // m_children ends up with two entries for the same Component →
    // hierarchy panel shows duplicates and `forEachChild` visits
    // twice. Runtime importer builds children in a single
    // monotonic pass and never double-adds, so the cost is editor-
    // only.
    if (std::find(m_children.begin(), m_children.end(), component) !=
        m_children.end())
    {
        return;
    }
#endif
    m_children.push_back(component);
}

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

void ContainerComponent::forEachChild(std::function<bool(Component*)> predicate)
{
    for (Component* child : m_children)
    {
        if (!predicate(child))
        {
            continue;
        }
        if (child->is<ContainerComponent>())
        {
            child->as<ContainerComponent>()->forEachChild(predicate);
        }
    }
}