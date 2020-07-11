#ifndef _RIVE_CUBIC_INTERPOLATOR_BASE_HPP_
#define _RIVE_CUBIC_INTERPOLATOR_BASE_HPP_
#include "core.hpp"
namespace rive
{
	class CubicInterpolatorBase : public Core
	{
	private:
		double m_X1 = 0.42;
		double m_Y1 = 0;
		double m_X2 = 0.58;
		double m_Y2 = 1;
	public:
		double x1() const { return m_X1; }
		void x1(double value) { m_X1 = value; }

		double y1() const { return m_Y1; }
		void y1(double value) { m_Y1 = value; }

		double x2() const { return m_X2; }
		void x2(double value) { m_X2 = value; }

		double y2() const { return m_Y2; }
		void y2(double value) { m_Y2 = value; }
	};
} // namespace rive

#endif