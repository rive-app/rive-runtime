#ifndef _RIVE_FILL_HPP_
#define _RIVE_FILL_HPP_
#include "generated/shapes/paint/fill_base.hpp"
#include <stdio.h>
namespace rive
{
	class Fill : public FillBase
	{
	public:
		Fill() { printf("Constructing Fill\n"); }
	};
} // namespace rive

#endif