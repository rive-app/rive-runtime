#ifndef _RIVE_BACKBOARD_BASE_HPP_
#define _RIVE_BACKBOARD_BASE_HPP_
#include "core.hpp"
namespace rive
{
	class BackboardBase : public Core
	{
	public:
		static const int typeKey = 23;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case BackboardBase::typeKey:
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