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
		int coreType() const override { return typeKey; }
		static const int isVisiblePropertyKey = 41;

	private:
		bool m_IsVisible = true;
	public:
		bool isVisible() const { return m_IsVisible; }
		void isVisible(bool value) { m_IsVisible = value; }

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
	};
} // namespace rive

#endif