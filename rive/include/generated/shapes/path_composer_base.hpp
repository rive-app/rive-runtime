#ifndef _RIVE_PATH_COMPOSER_BASE_HPP_
#define _RIVE_PATH_COMPOSER_BASE_HPP_
#include "component.hpp"
namespace rive
{
	class PathComposerBase : public Component
	{
	public:
		static const int typeKey = 9;
		int coreType() const override { return typeKey; }
	};
} // namespace rive

#endif