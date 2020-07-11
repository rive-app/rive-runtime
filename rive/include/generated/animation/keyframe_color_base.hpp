#ifndef _RIVE_KEY_FRAME_COLOR_BASE_HPP_
#define _RIVE_KEY_FRAME_COLOR_BASE_HPP_
#include "animation/keyframe.hpp"
#include "core/field_types/core_color_type.hpp"
namespace rive
{
	class KeyFrameColorBase : public KeyFrame
	{
	public:
		static const int typeKey = 37;
		int coreType() const override { return typeKey; }
		static const int valuePropertyKey = 88;

	private:
		int m_Value = 0;
	public:
		int value() const { return m_Value; }
		void value(int value) { m_Value = value; }

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case valuePropertyKey:
					m_Value = CoreColorType::deserialize(reader);
					return true;
			}
			return KeyFrame::deserialize(propertyKey, reader);
		}
	};
} // namespace rive

#endif