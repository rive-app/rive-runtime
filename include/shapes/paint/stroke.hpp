#ifndef _RIVE_STROKE_HPP_
#define _RIVE_STROKE_HPP_
#include "generated/shapes/paint/stroke_base.hpp"
#include "shapes/path_space.hpp"
namespace rive
{
	class Stroke : public StrokeBase
	{
	public:
		RenderPaint* initPaintMutator(ShapePaintMutator* mutator) override;
		PathSpace pathSpace() const override;
		void draw(Renderer* renderer, RenderPath* path) override;

	protected:
		void thicknessChanged() override;
		void capChanged() override;
		void joinChanged() override;
	};
} // namespace rive

#endif