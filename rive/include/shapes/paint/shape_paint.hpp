#ifndef _RIVE_SHAPE_PAINT_HPP_
#define _RIVE_SHAPE_PAINT_HPP_
#include "generated/shapes/paint/shape_paint_base.hpp"
#include "shapes/paint/shape_paint_mutator.hpp"
#include "shapes/path_space.hpp"
#include "renderer.hpp"

namespace rive
{
	class RenderPaint;
	class ShapePaintMutator;
	class ShapePaint : public ShapePaintBase
	{
	protected:
		RenderPaint* m_RenderPaint = nullptr;
		ShapePaintMutator* m_PaintMutator = nullptr;

	public:
		void onAddedClean(CoreContext* context) override;

		float renderOpacity() const { return m_PaintMutator->renderOpacity(); }
		void renderOpacity(float value) { m_PaintMutator->renderOpacity(value); }
		virtual RenderPaint* initPaintMutator(ShapePaintMutator* mutator);

		virtual PathSpace pathSpace() const = 0;

		virtual void draw(Renderer* renderer, RenderPath* path) = 0;
	};
} // namespace rive

#endif