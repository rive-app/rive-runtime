#ifndef _RIVE_STATE_MACHINE_LAYER_COMPONENT_BASE_HPP_
#define _RIVE_STATE_MACHINE_LAYER_COMPONENT_BASE_HPP_
#include "core.hpp"
namespace rive
{
	class StateMachineLayerComponentBase : public Core
	{
	protected:
		typedef Core Super;

	public:
		static const int typeKey = 66;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case StateMachineLayerComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
			}
			return false;
		}

	protected:
	};
} // namespace rive

#endif