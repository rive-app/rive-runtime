#ifndef _RIVE_KEY_FRAME_BASE_HPP_
#define _RIVE_KEY_FRAME_BASE_HPP_
#include "core.hpp"
#include "core/field_types/core_int_type.hpp"
namespace rive
{
	class KeyFrameBase : public Core
	{
	public:
		static const int typeKey = 29;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case KeyFrameBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int framePropertyKey = 67;
		static const int interpolationTypePropertyKey = 68;
		static const int interpolatorIdPropertyKey = 69;

	private:
		int m_Frame = 0;
		int m_InterpolationType = 0;
		int m_InterpolatorId = 0;
	public:
		int frame() const { return m_Frame; }
		void frame(int value) { m_Frame = value; }

		int interpolationType() const { return m_InterpolationType; }
		void interpolationType(int value) { m_InterpolationType = value; }

		int interpolatorId() const { return m_InterpolatorId; }
		void interpolatorId(int value) { m_InterpolatorId = value; }

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case framePropertyKey:
					m_Frame = CoreIntType::deserialize(reader);
					return true;
				case interpolationTypePropertyKey:
					m_InterpolationType = CoreIntType::deserialize(reader);
					return true;
				case interpolatorIdPropertyKey:
					m_InterpolatorId = CoreIntType::deserialize(reader);
					return true;
			}
			return false;
		}
	};
} // namespace rive

#endif