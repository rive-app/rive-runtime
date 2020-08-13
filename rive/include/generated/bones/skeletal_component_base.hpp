#ifndef _RIVE_SKELETAL_COMPONENT_BASE_HPP_
#define _RIVE_SKELETAL_COMPONENT_BASE_HPP_
#include "transform_component.hpp"
namespace rive
{
	class SkeletalComponentBase : public TransformComponent
	{
	protected:
		typedef TransformComponent Super;

	public:
		static const int typeKey = 39;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case SkeletalComponentBase::typeKey:
				case TransformComponentBase::typeKey:
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