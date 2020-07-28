#ifndef _RIVE_CONTAINER_COMPONENT_BASE_HPP_
#define _RIVE_CONTAINER_COMPONENT_BASE_HPP_
#include "component.hpp"
namespace rive
{
	class ContainerComponentBase : public Component
	{
	protected:
		typedef Component Super;

	public:
		static const int typeKey = 11;

		// Helper to quickly determine if a core object extends another without
		// RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case ContainerComponentBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

	protected:
	};
} // namespace rive

#endif