#ifndef _RIVE_BACKBOARD_HPP_
#define _RIVE_BACKBOARD_HPP_
#include "generated/backboard_base.hpp"
#include <stdio.h>
namespace rive
{
	class Backboard : public BackboardBase
	{
	public:
		Backboard() { printf("Constructing Backboard\n"); }
	};
} // namespace rive

#endif