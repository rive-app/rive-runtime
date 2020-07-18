#ifndef _RIVE_CUBIC_INTERPOLATOR_HPP_
#define _RIVE_CUBIC_INTERPOLATOR_HPP_
#include "generated/animation/cubic_interpolator_base.hpp"
namespace rive
{
	class CubicInterpolator : public CubicInterpolatorBase
	{
	public:
		void onAddedClean(CoreContext* context) override {}
		void onAddedDirty(CoreContext* context) override {}
	};
} // namespace rive

#endif