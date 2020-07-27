#ifndef _RIVE_SOLID_COLOR_HPP_
#define _RIVE_SOLID_COLOR_HPP_
#include "generated/shapes/paint/solid_color_base.hpp"
#include "shapes/paint/shape_paint_mutator.hpp"
namespace rive
{
	class SolidColor : public SolidColorBase, public ShapePaintMutator
	{
	public:
		void onAddedDirty(CoreContext* context) override;
		void onAddedClean(CoreContext* context) override {}
	protected:
		void renderOpacityChanged() override;
		void colorValueChanged() override;
	};
} // namespace rive

#endif