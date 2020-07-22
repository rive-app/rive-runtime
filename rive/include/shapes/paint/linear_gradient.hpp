#ifndef _RIVE_LINEAR_GRADIENT_HPP_
#define _RIVE_LINEAR_GRADIENT_HPP_
#include "generated/shapes/paint/linear_gradient_base.hpp"
#include "shapes/paint/shape_paint_mutator.hpp"
namespace rive
{
	class LinearGradient : public LinearGradientBase, public ShapePaintMutator
	{
	public:
		void onAddedDirty(CoreContext* context) override;
		void onAddedClean(CoreContext* context) override {}

	protected:
		void syncColor() override;
	};
} // namespace rive

#endif