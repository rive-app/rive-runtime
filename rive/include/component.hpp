#ifndef _RIVE_COMPONENT_HPP_
#define _RIVE_COMPONENT_HPP_
#include "component_dirt.hpp"
#include "generated/component_base.hpp"

#include <vector>

namespace rive
{
	class ContainerComponent;
	class Artboard;

	class Component : public ComponentBase
	{
		friend class Artboard;

	private:
		ContainerComponent* m_Parent;
		std::vector<Component*> m_Dependents;

		unsigned int m_GraphOrder;
		ComponentDirt m_Dirt = ComponentDirt::Filthy;

	public:
		void onAddedDirty(CoreContext* context) override;
		ContainerComponent* parent() const;
		const std::vector<Component*>& dependents() const { return m_Dependents; }
		void addDependent(Component* component);

		// TODO: re-evaluate when more of the lib is complete...
		// These could be pure virtual but we define them empty here to avoid
		// having to implement them in a bunch of concrete classes that
		// currently don't use this logic.
		virtual void buildDependencies() {}
		virtual void onDirty(ComponentDirt dirt) {}
		virtual void update(ComponentDirt value) {}

		unsigned int graphOrder() const { return m_GraphOrder; }
		bool addDirt(ComponentDirt value, bool recurse = false);
		bool hasDirt(ComponentDirt value) const { return (m_Dirt & value) == value; }
	};
} // namespace rive

#endif
