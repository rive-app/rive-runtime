#ifndef _RIVE_RADIAL_GRADIENT_HPP_
#define _RIVE_RADIAL_GRADIENT_HPP_
#include "generated/shapes/paint/radial_gradient_base.hpp"
namespace rive
{
	class RadialGradient : public RadialGradientBase
	{
	public:
		void onAddedClean(CoreContext* context) override {}
	};
} // namespace rive

#endif