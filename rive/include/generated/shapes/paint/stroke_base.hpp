#ifndef _RIVE_STROKE_BASE_HPP_
#define _RIVE_STROKE_BASE_HPP_
#include "core/field_types/core_bool_type.hpp"
#include "core/field_types/core_double_type.hpp"
#include "core/field_types/core_int_type.hpp"
#include "shapes/paint/shape_paint.hpp"
namespace rive
{
	class StrokeBase : public ShapePaint
	{
	public:
		static const int typeKey = 24;
		int coreType() const override { return typeKey; }
		static const int thicknessPropertyKey = 47;
		static const int capPropertyKey = 48;
		static const int joinPropertyKey = 49;
		static const int transformAffectsStrokePropertyKey = 50;

	private:
		double m_Thickness = 1;
		int m_Cap = 0;
		int m_Join = 0;
		bool m_TransformAffectsStroke = true;
	public:
		double thickness() const { return m_Thickness; }
		void thickness(double value) { m_Thickness = value; }

		int cap() const { return m_Cap; }
		void cap(int value) { m_Cap = value; }

		int join() const { return m_Join; }
		void join(int value) { m_Join = value; }

		bool transformAffectsStroke() const { return m_TransformAffectsStroke; }
		void transformAffectsStroke(bool value) { m_TransformAffectsStroke = value; }

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case thicknessPropertyKey:
					m_Thickness = CoreDoubleType::deserialize(reader);
					return true;
				case capPropertyKey:
					m_Cap = CoreIntType::deserialize(reader);
					return true;
				case joinPropertyKey:
					m_Join = CoreIntType::deserialize(reader);
					return true;
				case transformAffectsStrokePropertyKey:
					m_TransformAffectsStroke = CoreBoolType::deserialize(reader);
					return true;
			}
			return ShapePaint::deserialize(propertyKey, reader);
		}
	};
} // namespace rive

#endif