#include "component.hpp"
#include "container_component.hpp"
#include "core_context.hpp"

using namespace rive;

ContainerComponent* Component::parent() const { return m_Parent; }

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