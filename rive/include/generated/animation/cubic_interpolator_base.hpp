#ifndef _RIVE_CUBIC_INTERPOLATOR_BASE_HPP_
#define _RIVE_CUBIC_INTERPOLATOR_BASE_HPP_
#include "core.hpp"
#include "core/field_types/core_double_type.hpp"
namespace rive
{
	class CubicInterpolatorBase : public Core
	{
	protected:
		typedef Core Super;

	public:
		static const int typeKey = 28;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case CubicInterpolatorBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int x1PropertyKey = 63;
		static const int y1PropertyKey = 64;
		static const int x2PropertyKey = 65;
		static const int y2PropertyKey = 66;

	private:
		float m_X1 = 0.42;
		float m_Y1 = 0;
		float m_X2 = 0.58;
		float m_Y2 = 1;
	public:
		inline float x1() const { return m_X1; }
		void x1(float value)
		{
			if (m_X1 == value)
			{
				return;
			}
			m_X1 = value;
			x1Changed();
		}

		inline float y1() const { return m_Y1; }
		void y1(float value)
		{
			if (m_Y1 == value)
			{
				return;
			}
			m_Y1 = value;
			y1Changed();
		}

		inline float x2() const { return m_X2; }
		void x2(float value)
		{
			if (m_X2 == value)
			{
				return;
			}
			m_X2 = value;
			x2Changed();
		}

		inline float y2() const { return m_Y2; }
		void y2(float value)
		{
			if (m_Y2 == value)
			{
				return;
			}
			m_Y2 = value;
			y2Changed();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case x1PropertyKey:
					m_X1 = CoreDoubleType::deserialize(reader);
					return true;
				case y1PropertyKey:
					m_Y1 = CoreDoubleType::deserialize(reader);
					return true;
				case x2PropertyKey:
					m_X2 = CoreDoubleType::deserialize(reader);
					return true;
				case y2PropertyKey:
					m_Y2 = CoreDoubleType::deserialize(reader);
					return true;
			}
			return false;
		}

	protected:
		virtual void x1Changed() {}
		virtual void y1Changed() {}
		virtual void x2Changed() {}
		virtual void y2Changed() {}
	};
} // namespace rive

#endif