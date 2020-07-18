#ifndef _RIVE_LINEAR_GRADIENT_HPP_
#define _RIVE_LINEAR_GRADIENT_HPP_
#include "generated/shapes/paint/linear_gradient_base.hpp"
namespace rive
{
	class LinearGradient : public LinearGradientBase
	{
	public:
		void onAddedClean(CoreContext* context) override {}
	};
} // namespace rive

#endif