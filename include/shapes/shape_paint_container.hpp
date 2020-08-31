#ifndef _RIVE_SHAPE_PAINT_CONTAINER_HPP_
#define _RIVE_SHAPE_PAINT_CONTAINER_HPP_
#include <vector>
#include "shapes/path_space.hpp"
#include "shapes/paint/blend_mode.hpp"

namespace rive
{
	class ShapePaint;
	class Component;
	class RenderPath;
	
	class ShapePaintContainer
	{
		friend class ShapePaint;

	protected:
		PathSpace m_DefaultPathSpace = PathSpace::Neither;
		std::vector<ShapePaint*> m_ShapePaints;
		void addPaint(ShapePaint* paint);

		// TODO: void draw(Renderer* renderer, PathComposer& composer);
	public:
		static ShapePaintContainer* from(Component* component);

        PathSpace pathSpace() const;

		void invalidateStrokeEffects();

		RenderPath* makeRenderPath(PathSpace space);
	};
} // namespace rive

#endif