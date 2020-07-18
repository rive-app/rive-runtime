#ifndef _RIVE_SOLID_COLOR_HPP_
#define _RIVE_SOLID_COLOR_HPP_
#include "generated/shapes/paint/solid_color_base.hpp"
namespace rive
{
	class SolidColor : public SolidColorBase
	{
	public:
		void onAddedClean(CoreContext* context) {}
	};
} // namespace rive

#endif