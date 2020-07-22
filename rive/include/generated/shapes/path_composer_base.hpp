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
		static const int typeKey = 9;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
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

		int coreType() const override { return typeKey; }

	protected:
	};
} // namespace rive

#endif