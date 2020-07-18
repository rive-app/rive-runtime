#ifndef _RIVE_STROKE_HPP_
#define _RIVE_STROKE_HPP_
#include "generated/shapes/paint/stroke_base.hpp"
namespace rive
{
	class Stroke : public StrokeBase
	{
	public:
		void onAddedClean(CoreContext* context) override {}
	};
} // namespace rive

#endif