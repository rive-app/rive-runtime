#ifndef _RIVE_SOLID_COLOR_BASE_HPP_
#define _RIVE_SOLID_COLOR_BASE_HPP_
#include "component.hpp"
#include "core/field_types/core_color_type.hpp"
namespace rive
{
	class SolidColorBase : public Component
	{
	protected:
		typedef Component Super;

	public:
		static const int typeKey = 18;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case SolidColorBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int colorValuePropertyKey = 37;

	private:
		int m_ColorValue = 0xFF747474;
	public:
		int colorValue() const { return m_ColorValue; }
		void colorValue(int value)
		{
			if (m_ColorValue == value)
			{
				return;
			}
			m_ColorValue = value;
			colorValueChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case colorValuePropertyKey:
					m_ColorValue = CoreColorType::deserialize(reader);
					return true;
			}
			return Component::deserialize(propertyKey, reader);
		}

	protected:
		virtual void colorValueChanged() {}
	};
} // namespace rive

#endif