#ifndef _RIVE_CONTAINER_COMPONENT_BASE_HPP_
#define _RIVE_CONTAINER_COMPONENT_BASE_HPP_
#include "component.hpp"
namespace rive
{
	class ContainerComponentBase : public Component
	{
	public:
		static const int typeKey = 11;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool inheritsFrom(int typeKey) override
		{
			switch (typeKey)
			{
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }
	};
} // namespace rive

#endif