#ifndef _RIVE_RECTANGLE_BASE_HPP_
#define _RIVE_RECTANGLE_BASE_HPP_
#include "shapes/parametric_path.hpp"
namespace rive
{
	class RectangleBase : public ParametricPath
	{
	private:
		double m_CornerRadius = 0.0;
	public:
		double cornerRadius() const { return m_CornerRadius; }
		void cornerRadius(double value) { m_CornerRadius = value; }
	};
} // namespace rive

#endif