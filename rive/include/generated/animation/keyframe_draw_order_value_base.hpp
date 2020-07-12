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
		bool inheritsFrom(int typeKey) override { return false; }

		int coreType() const override { return typeKey; }

		static const int drawableIdPropertyKey = 77;
		static const int valuePropertyKey = 78;

	private:
		int m_DrawableId = 0;
		int m_Value = 0;
	public:
		int drawableId() const { return m_DrawableId; }
		void drawableId(int value) { m_DrawableId = value; }

		int value() const { return m_Value; }
		void value(int value) { m_Value = value; }

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
	};
} // namespace rive

#endif