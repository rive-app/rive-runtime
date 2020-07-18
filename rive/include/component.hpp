#ifndef _RIVE_COMPONENT_HPP_
#define _RIVE_COMPONENT_HPP_
#include "generated/component_base.hpp"

namespace rive
{
	class ContainerComponent;
	class Component : public ComponentBase
	{
	private:
		ContainerComponent* m_Parent;

	public:
		void onAddedDirty(CoreContext* context) override;
		ContainerComponent* parent() const;
	};
} // namespace rive

#endif