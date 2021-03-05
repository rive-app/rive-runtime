#ifndef _RIVE_TRANSITION_DOUBLE_CONDITION_BASE_HPP_
#define _RIVE_TRANSITION_DOUBLE_CONDITION_BASE_HPP_
#include "animation/transition_value_condition.hpp"
#include "core/field_types/core_double_type.hpp"
namespace rive
{
	class TransitionDoubleConditionBase : public TransitionValueCondition
	{
	protected:
		typedef TransitionValueCondition Super;

	public:
		static const int typeKey = 70;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case TransitionDoubleConditionBase::typeKey:
				case TransitionValueConditionBase::typeKey:
				case TransitionConditionBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int valuePropertyKey = 157;

	private:
		float m_Value = 0;
	public:
		inline float value() const { return m_Value; }
		void value(float value)
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
			return TransitionValueCondition::deserialize(propertyKey, reader);
		}

	protected:
		virtual void valueChanged() {}
	};
} // namespace rive

#endif