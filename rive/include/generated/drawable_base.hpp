#ifndef _RIVE_DRAWABLE_BASE_HPP_
#define _RIVE_DRAWABLE_BASE_HPP_
#include "node.hpp"
namespace rive
{
	class DrawableBase : public Node
	{
	private:
		int m_DrawOrder = 0;
		int m_BlendMode = 0;
	public:
		int drawOrder() const { return m_DrawOrder; }
		void drawOrder(int value) { m_DrawOrder = value; }

		int blendMode() const { return m_BlendMode; }
		void blendMode(int value) { m_BlendMode = value; }
	};
} // namespace rive

#endif