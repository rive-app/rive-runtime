#ifndef _RIVE_SHAPE_PAINT_CONTAINER_HPP_
#define _RIVE_SHAPE_PAINT_CONTAINER_HPP_
#include <vector>
#include "shapes/path_space.hpp"
namespace rive
{
	class ShapePaint;
	class Component;

	class ShapePaintContainer
	{
		friend class ShapePaint;

	protected:
		std::vector<ShapePaint*> m_ShapePaints;
		void addPaint(ShapePaint* paint);

		// TODO: void draw(Renderer* renderer, PathComposer& composer);
	public:
		static ShapePaintContainer* from(Component* component);

        PathSpace pathSpace() const;
	};
} // namespace rive

#endif