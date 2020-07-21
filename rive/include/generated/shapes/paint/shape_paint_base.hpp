#ifndef _RIVE_SHAPE_PAINT_BASE_HPP_
#define _RIVE_SHAPE_PAINT_BASE_HPP_
#include "container_component.hpp"
#include "core/field_types/core_bool_type.hpp"
namespace rive
{
	class ShapePaintBase : public ContainerComponent
	{
	public:
		static const int typeKey = 21;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case ShapePaintBase::typeKey:
				case ContainerComponentBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int isVisiblePropertyKey = 41;

	private:
		bool m_IsVisible = true;
	public:
		bool isVisible() const { return m_IsVisible; }
		void isVisible(bool value)
		{
			if (m_IsVisible == value)
			{
				return;
			}
			m_IsVisible = value;
			isVisibleChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case isVisiblePropertyKey:
					m_IsVisible = CoreBoolType::deserialize(reader);
					return true;
			}
			return ContainerComponent::deserialize(propertyKey, reader);
		}

	protected:
		virtual void isVisibleChanged() {}
	};
} // namespace rive

#endif