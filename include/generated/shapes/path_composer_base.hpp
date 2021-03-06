#ifndef _RIVE_PATH_COMPOSER_BASE_HPP_
#define _RIVE_PATH_COMPOSER_BASE_HPP_
#include "component.hpp"
namespace rive
{
	class PathComposerBase : public Component
	{
	protected:
		typedef Component Super;

	public:
		static const uint16_t typeKey = 9;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(uint16_t typeKey) const override
		{
			switch (typeKey)
			{
				case PathComposerBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		uint16_t coreType() const override { return typeKey; }

	protected:
	};
} // namespace rive

#endif