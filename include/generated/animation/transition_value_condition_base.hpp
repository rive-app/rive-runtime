#ifndef _RIVE_TRANSITION_VALUE_CONDITION_BASE_HPP_
#define _RIVE_TRANSITION_VALUE_CONDITION_BASE_HPP_
#include "animation/transition_condition.hpp"
#include "core/field_types/core_uint_type.hpp"
namespace rive
{
	class TransitionValueConditionBase : public TransitionCondition
	{
	protected:
		typedef TransitionCondition Super;

	public:
		static const int typeKey = 69;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case TransitionValueConditionBase::typeKey:
				case TransitionConditionBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int opValuePropertyKey = 156;

	private:
		int m_OpValue = 0;
	public:
		inline int opValue() const { return m_OpValue; }
		void opValue(int value)
		{
			if (m_OpValue == value)
			{
				return;
			}
			m_OpValue = value;
			opValueChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case opValuePropertyKey:
					m_OpValue = CoreUintType::deserialize(reader);
					return true;
			}
			return TransitionCondition::deserialize(propertyKey, reader);
		}

	protected:
		virtual void opValueChanged() {}
	};
} // namespace rive

#endif