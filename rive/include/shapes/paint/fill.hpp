#ifndef _RIVE_FILL_HPP_
#define _RIVE_FILL_HPP_
#include "generated/shapes/paint/fill_base.hpp"
#include "shapes/path_space.hpp"
namespace rive
{
	class Fill : public FillBase
	{
	public:
		RenderPaint* initPaintMutator(ShapePaintMutator* mutator) override;
		PathSpace pathSpace() const override;
		void draw(Renderer* renderer, RenderPath* path) override;
	};
} // namespace rive

#endif