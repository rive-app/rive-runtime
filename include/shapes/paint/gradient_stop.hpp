#ifndef _RIVE_GRADIENT_STOP_HPP_
#define _RIVE_GRADIENT_STOP_HPP_
#include "generated/shapes/paint/gradient_stop_base.hpp"
namespace rive
{
	class GradientStop : public GradientStopBase
	{
	public:
		StatusCode onAddedDirty(CoreContext* context) override;
		StatusCode onAddedClean(CoreContext* context) override
		{
			return StatusCode::Ok;
		}

	protected:
		void colorValueChanged() override;
		void positionChanged() override;
	};
} // namespace rive

#endif