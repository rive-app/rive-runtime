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
	protected:
		typedef ShapePaint Super;

	public:
		static const int typeKey = 24;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case StrokeBase::typeKey:
				case ShapePaintBase::typeKey:
				case ContainerComponentBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

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
		void thickness(double value)
		{
			if (m_Thickness == value)
			{
				return;
			}
			m_Thickness = value;
			thicknessChanged();
		}

		int cap() const { return m_Cap; }
		void cap(int value)
		{
			if (m_Cap == value)
			{
				return;
			}
			m_Cap = value;
			capChanged();
		}

		int join() const { return m_Join; }
		void join(int value)
		{
			if (m_Join == value)
			{
				return;
			}
			m_Join = value;
			joinChanged();
		}

		bool transformAffectsStroke() const { return m_TransformAffectsStroke; }
		void transformAffectsStroke(bool value)
		{
			if (m_TransformAffectsStroke == value)
			{
				return;
			}
			m_TransformAffectsStroke = value;
			transformAffectsStrokeChanged();
		}

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

	protected:
		virtual void thicknessChanged() {}
		virtual void capChanged() {}
		virtual void joinChanged() {}
		virtual void transformAffectsStrokeChanged() {}
	};
} // namespace rive

#endif