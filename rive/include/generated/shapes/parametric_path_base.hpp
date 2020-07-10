#ifndef _RIVE_PARAMETRIC_PATH_BASE_HPP_
#define _RIVE_PARAMETRIC_PATH_BASE_HPP_
#include "shapes/path.hpp"
namespace rive
{
	class ParametricPathBase : public Path
	{
	private:
		double m_Width = 0;
		double m_Height = 0;
	public:
		double width() const { return m_Width; }
		void width(double value) { m_Width = value; }

		double height() const { return m_Height; }
		void height(double value) { m_Height = value; }
	};
} // namespace rive

#endif