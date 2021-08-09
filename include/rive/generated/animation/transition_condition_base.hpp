#ifndef _RIVE_TRANSITION_CONDITION_BASE_HPP_
#define _RIVE_TRANSITION_CONDITION_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
	class TransitionConditionBase : public Core
	{
	protected:
		typedef Core Super;

	public:
		static const uint16_t typeKey = 67;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(uint16_t typeKey) const override
		{
			switch (typeKey)
			{
				case TransitionConditionBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		uint16_t coreType() const override { return typeKey; }

		static const uint16_t inputIdPropertyKey = 155;

	private:
		int m_InputId = -1;
	public:
		inline int inputId() const { return m_InputId; }
		void inputId(int value)
		{
			if (m_InputId == value)
			{
				return;
			}
			m_InputId = value;
			inputIdChanged();
		}

		void copy(const TransitionConditionBase& object)
		{
			m_InputId = object.m_InputId;
		}

		bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case inputIdPropertyKey:
					m_InputId = CoreUintType::deserialize(reader);
					return true;
			}
			return false;
		}

	protected:
		virtual void inputIdChanged() {}
	};
} // namespace rive

#endif