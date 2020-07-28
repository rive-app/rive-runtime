#ifndef _RIVE_GRADIENT_STOP_BASE_HPP_
#define _RIVE_GRADIENT_STOP_BASE_HPP_
#include "component.hpp"
#include "core/field_types/core_color_type.hpp"
#include "core/field_types/core_double_type.hpp"
namespace rive
{
	class GradientStopBase : public Component
	{
	protected:
		typedef Component Super;

	public:
		static const int typeKey = 19;

		// Helper to quickly determine if a core object extends another without
		// RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case GradientStopBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int colorValuePropertyKey = 38;
		static const int positionPropertyKey = 39;

	private:
		int m_ColorValue = 0xFFFFFFFF;
		float m_Position = 0;
	public:
		inline int colorValue() const { return m_ColorValue; }
		void colorValue(int value)
		{
			if (m_ColorValue == value)
			{
				return;
			}
			m_ColorValue = value;
			colorValueChanged();
		}

		inline float position() const { return m_Position; }
		void position(float value)
		{
			if (m_Position == value)
			{
				return;
			}
			m_Position = value;
			positionChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case colorValuePropertyKey:
					m_ColorValue = CoreColorType::deserialize(reader);
					return true;
				case positionPropertyKey:
					m_Position = CoreDoubleType::deserialize(reader);
					return true;
			}
			return Component::deserialize(propertyKey, reader);
		}

	protected:
		virtual void colorValueChanged() {}
		virtual void positionChanged() {}
	};
} // namespace rive

#endif