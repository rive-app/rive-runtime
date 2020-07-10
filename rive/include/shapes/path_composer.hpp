#ifndef _RIVE_PATH_COMPOSER_HPP_
#define _RIVE_PATH_COMPOSER_HPP_
#include "generated/shapes/path_composer_base.hpp"
#include <stdio.h>
namespace rive
{
	class PathComposer : public PathComposerBase
	{
	public:
		PathComposer() { printf("Constructing PathComposer\n"); }
	};
} // namespace rive

#endif