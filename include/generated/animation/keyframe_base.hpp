#ifndef _RIVE_KEY_FRAME_BASE_HPP_
#define _RIVE_KEY_FRAME_BASE_HPP_
#include "core.hpp"
#include "core/field_types/core_uint_type.hpp"
namespace rive
{
	class KeyFrameBase : public Core
	{
	protected:
		typedef Core Super;

	public:
		static const int typeKey = 29;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
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
		inline int frame() const { return m_Frame; }
		void frame(int value)
		{
			if (m_Frame == value)
			{
				return;
			}
			m_Frame = value;
			frameChanged();
		}

		inline int interpolationType() const { return m_InterpolationType; }
		void interpolationType(int value)
		{
			if (m_InterpolationType == value)
			{
				return;
			}
			m_InterpolationType = value;
			interpolationTypeChanged();
		}

		inline int interpolatorId() const { return m_InterpolatorId; }
		void interpolatorId(int value)
		{
			if (m_InterpolatorId == value)
			{
				return;
			}
			m_InterpolatorId = value;
			interpolatorIdChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case framePropertyKey:
					m_Frame = CoreUintType::deserialize(reader);
					return true;
				case interpolationTypePropertyKey:
					m_InterpolationType = CoreUintType::deserialize(reader);
					return true;
				case interpolatorIdPropertyKey:
					m_InterpolatorId = CoreUintType::deserialize(reader);
					return true;
			}
			return false;
		}

	protected:
		virtual void frameChanged() {}
		virtual void interpolationTypeChanged() {}
		virtual void interpolatorIdChanged() {}
	};
} // namespace rive

#endif