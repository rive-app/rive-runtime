#ifndef _RIVE_CONTAINER_COMPONENT_HPP_
#define _RIVE_CONTAINER_COMPONENT_HPP_
#include "generated/container_component_base.hpp"
#include <stdio.h>
namespace rive
{
	class ContainerComponent : public ContainerComponentBase
	{
	public:
		ContainerComponent() { printf("Constructing ContainerComponent\n"); }
	};
} // namespace rive

#endif