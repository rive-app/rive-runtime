#ifndef _RIVE_PATH_HPP_
#define _RIVE_PATH_HPP_
#include "generated/shapes/path_base.hpp"
#include <stdio.h>
namespace rive
{
	class Path : public PathBase
	{
	public:
		Path() { printf("Constructing Path\n"); }
	};
} // namespace rive

#endif