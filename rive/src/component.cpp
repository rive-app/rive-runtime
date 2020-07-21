#include "component.hpp"
#include "container_component.hpp"
#include "core_context.hpp"
#include <algorithm>

using namespace rive;

void Component::onAddedDirty(CoreContext* context)
{
	auto coreObject = context->resolve(parentId());
	if (coreObject == nullptr)
	{
		return;
	}
	if (coreObject->is<ContainerComponent>())
	{
		m_Parent = reinterpret_cast<ContainerComponent*>(coreObject);
	}
}

void Component::addDependent(Component* component)
{
	// Make it's not already a dependent.
	assert(std::find(m_Dependents.begin(), m_Dependents.end(), component) == m_Dependents.end());
	m_Dependents.push_back(component);
}

bool Component::addDirt(ComponentDirt value, bool recurse)
{
	if ((m_Dirt & value) == value)
	{
		// Already marked.
		return false;
	}

	// Make sure dirt is set before calling anything that can set more dirt.
	m_Dirt |= value;

	onDirty(m_Dirt);
	// artboard ?.onComponentDirty(this);

	if (!recurse)
	{
		return true;
	}

	for (auto d : m_Dependents)
	{
		d->addDirt(value, true);
	}
	return true;
}