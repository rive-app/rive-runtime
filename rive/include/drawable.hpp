#ifndef _RIVE_DRAWABLE_HPP_
#define _RIVE_DRAWABLE_HPP_
#include "generated/drawable_base.hpp"
#include "renderer.hpp"
#include <vector>

namespace rive
{
	class ClippingShape;
	class Drawable : public DrawableBase
	{
	private:
		std::vector<ClippingShape*> m_ClippingShapes;

	public:
		bool clip(Renderer* renderer) const;
		virtual void draw(Renderer* renderer) = 0;
		void addClippingShape(ClippingShape* shape);
		inline const std::vector<ClippingShape*>& clippingShapes() const
		{
			return m_ClippingShapes;
		}

	protected:
		void drawOrderChanged() override;
	};
} // namespace rive

#endif