#ifndef _RIVE_DRAWABLE_HPP_
#define _RIVE_DRAWABLE_HPP_
#include "generated/drawable_base.hpp"
#include "renderer.hpp"
#include <vector>

namespace rive
{
	class ClippingShape;
	class Artboard;
	class DrawRules;
	
	class Drawable : public DrawableBase
	{
		friend class Artboard;

	private:
		std::vector<ClippingShape*> m_ClippingShapes;

		/// Used exclusively by the artboard;
		DrawRules* flattenedDrawRules = nullptr;
		Drawable* prev = nullptr;
		Drawable* next = nullptr;

	public:
		bool clip(Renderer* renderer) const;
		virtual void draw(Renderer* renderer) = 0;
		void addClippingShape(ClippingShape* shape);
		inline const std::vector<ClippingShape*>& clippingShapes() const
		{
			return m_ClippingShapes;
		}
	};
} // namespace rive

#endif