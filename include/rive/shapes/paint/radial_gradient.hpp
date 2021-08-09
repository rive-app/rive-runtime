#ifndef _RIVE_RADIAL_GRADIENT_HPP_
#define _RIVE_RADIAL_GRADIENT_HPP_
#include "rive/generated/shapes/paint/radial_gradient_base.hpp"
namespace rive
{
	class RadialGradient : public RadialGradientBase
	{
	public:
		void makeGradient(const Vec2D& start, const Vec2D& end) override;
	};
} // namespace rive

#endif