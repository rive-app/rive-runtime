#ifndef _RIVE_FILL_BASE_HPP_
#define _RIVE_FILL_BASE_HPP_
#include "shapes/paint/shape_paint.hpp"
namespace rive
{
	class FillBase : public ShapePaint
	{
	private:
		int m_FillRule = 0;
	public:
		int fillRule() const { return m_FillRule; }
		void fillRule(int value) { m_FillRule = value; }
	};
} // namespace rive

#endif