#ifndef _RIVE_KEY_FRAME_DOUBLE_BASE_HPP_
#define _RIVE_KEY_FRAME_DOUBLE_BASE_HPP_
#include "animation/keyframe.hpp"
#include "core/field_types/core_double_type.hpp"
namespace rive
{
	class KeyFrameDoubleBase : public KeyFrame
	{
	public:
		static const int typeKey = 30;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case KeyFrameDoubleBase::typeKey:
				case KeyFrameBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int valuePropertyKey = 70;

	private:
		double m_Value = 0.0;
	public:
		double value() const { return m_Value; }
		void value(double value)
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
				case valuePropertyKey:
					m_Value = CoreDoubleType::deserialize(reader);
					return true;
			}
			return KeyFrame::deserialize(propertyKey, reader);
		}

	protected:
		virtual void valueChanged() {}
	};
} // namespace rive

#endif