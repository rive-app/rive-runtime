#ifndef _RIVE_GRADIENT_STOP_HPP_
#define _RIVE_GRADIENT_STOP_HPP_
#include "generated/shapes/paint/gradient_stop_base.hpp"
namespace rive
{
	class GradientStop : public GradientStopBase
	{
	public:
		void onAddedDirty(CoreContext* context) override;
		void onAddedClean(CoreContext* context) override {}

	protected:
		void colorValueChanged() override;
		void positionChanged() override;
	};
} // namespace rive

#endif