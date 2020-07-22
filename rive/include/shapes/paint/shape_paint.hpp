#ifndef _RIVE_SHAPE_PAINT_HPP_
#define _RIVE_SHAPE_PAINT_HPP_
#include "generated/shapes/paint/shape_paint_base.hpp"
#include "shapes/paint/shape_paint_mutator.hpp"
#include "shapes/path_space.hpp"

namespace rive
{
	class RenderPaint;
	class ShapePaintMutator;
	class ShapePaint : public ShapePaintBase
	{
	private:
		RenderPaint* m_RenderPaint;
		ShapePaintMutator* m_PaintMutator;

	public:
		void onAddedDirty(CoreContext* context) override;
		void onAddedClean(CoreContext* context) override;

		float renderOpacity() const { return m_PaintMutator->renderOpacity(); }
		void renderOpacity(float value) { m_PaintMutator->renderOpacity(value); }
		RenderPaint* initPaintMutator(ShapePaintMutator* mutator);

		virtual PathSpace pathSpace() const = 0;
	};
} // namespace rive

#endif