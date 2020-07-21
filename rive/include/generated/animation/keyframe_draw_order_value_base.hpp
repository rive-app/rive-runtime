#ifndef _RIVE_KEY_FRAME_DRAW_ORDER_VALUE_BASE_HPP_
#define _RIVE_KEY_FRAME_DRAW_ORDER_VALUE_BASE_HPP_
#include "core.hpp"
#include "core/field_types/core_int_type.hpp"
namespace rive
{
	class KeyFrameDrawOrderValueBase : public Core
	{
	public:
		static const int typeKey = 33;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case KeyFrameDrawOrderValueBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int drawableIdPropertyKey = 77;
		static const int valuePropertyKey = 78;

	private:
		int m_DrawableId = 0;
		int m_Value = 0;
	public:
		int drawableId() const { return m_DrawableId; }
		void drawableId(int value)
		{
			if (m_DrawableId == value)
			{
				return;
			}
			m_DrawableId = value;
			drawableIdChanged();
		}

		int value() const { return m_Value; }
		void value(int value)
		{
			if (m_Value == value)
			{
				return;
			}
			m_Value = value;
			valueChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case drawableIdPropertyKey:
					m_DrawableId = CoreIntType::deserialize(reader);
					return true;
				case valuePropertyKey:
					m_Value = CoreIntType::deserialize(reader);
					return true;
			}
			return false;
		}

	protected:
		virtual void drawableIdChanged() {}
		virtual void valueChanged() {}
	};
} // namespace rive

#endif