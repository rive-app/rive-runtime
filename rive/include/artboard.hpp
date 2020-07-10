#ifndef _RIVE_ARTBOARD_HPP_
#define _RIVE_ARTBOARD_HPP_
#include "generated/artboard_base.hpp"
#include <stdio.h>
namespace rive
{
	class Artboard : public ArtboardBase
	{
	public:
		Artboard() { printf("Constructing Artboard\n"); }
	};
} // namespace rive

#endif