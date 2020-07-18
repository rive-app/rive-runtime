#ifndef _RIVE_PATH_COMPOSER_HPP_
#define _RIVE_PATH_COMPOSER_HPP_
#include "generated/shapes/path_composer_base.hpp"
namespace rive
{
	class PathComposer : public PathComposerBase
	{
	public:
		void onAddedClean(CoreContext* context) {}
	};
} // namespace rive

#endif