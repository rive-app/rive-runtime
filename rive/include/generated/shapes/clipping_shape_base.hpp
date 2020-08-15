#ifndef _RIVE_CLIPPING_SHAPE_BASE_HPP_
#define _RIVE_CLIPPING_SHAPE_BASE_HPP_
#include "component.hpp"
#include "core/field_types/core_bool_type.hpp"
#include "core/field_types/core_uint_type.hpp"
namespace rive
{
	class ClippingShapeBase : public Component
	{
	protected:
		typedef Component Super;

	public:
		static const int typeKey = 42;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case ClippingShapeBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int shapeIdPropertyKey = 92;
		static const int clipOpValuePropertyKey = 93;
		static const int isVisiblePropertyKey = 94;

	private:
		int m_ShapeId = 0;
		int m_ClipOpValue = 0;
		bool m_IsVisible = true;
	public:
		inline int shapeId() const { return m_ShapeId; }
		void shapeId(int value)
		{
			if (m_ShapeId == value)
			{
				return;
			}
			m_ShapeId = value;
			shapeIdChanged();
		}

		inline int clipOpValue() const { return m_ClipOpValue; }
		void clipOpValue(int value)
		{
			if (m_ClipOpValue == value)
			{
				return;
			}
			m_ClipOpValue = value;
			clipOpValueChanged();
		}

		inline bool isVisible() const { return m_IsVisible; }
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
				case shapeIdPropertyKey:
					m_ShapeId = CoreUintType::deserialize(reader);
					return true;
				case clipOpValuePropertyKey:
					m_ClipOpValue = CoreUintType::deserialize(reader);
					return true;
				case isVisiblePropertyKey:
					m_IsVisible = CoreBoolType::deserialize(reader);
					return true;
			}
			return Component::deserialize(propertyKey, reader);
		}

	protected:
		virtual void shapeIdChanged() {}
		virtual void clipOpValueChanged() {}
		virtual void isVisibleChanged() {}
	};
} // namespace rive

#endif