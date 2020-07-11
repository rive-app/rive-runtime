#ifndef _RIVE_CUBIC_INTERPOLATOR_BASE_HPP_
#define _RIVE_CUBIC_INTERPOLATOR_BASE_HPP_
#include "core.hpp"
#include "core/field_types/core_double_type.hpp"
namespace rive
{
	class CubicInterpolatorBase : public Core
	{
	public:
		static const int typeKey = 28;
		int coreType() const override { return typeKey; }
		static const int x1PropertyKey = 63;
		static const int y1PropertyKey = 64;
		static const int x2PropertyKey = 65;
		static const int y2PropertyKey = 66;

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

		bool deserialize(int propertyKey, BinaryReader& reader) override { return false; }
	};
} // namespace rive

#endif