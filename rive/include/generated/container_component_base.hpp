#ifndef _RIVE_CONTAINER_COMPONENT_BASE_HPP_
#define _RIVE_CONTAINER_COMPONENT_BASE_HPP_
#include "component.hpp"
namespace rive
{
	class ContainerComponentBase : public Component
	{
	public:
		static const int typeKey = 11;
		int coreType() const override { return typeKey; }
	};
} // namespace rive

#endif