#ifndef _RIVE_STATE_MACHINE_DOUBLE_BASE_HPP_
#define _RIVE_STATE_MACHINE_DOUBLE_BASE_HPP_
#include "animation/state_machine_input.hpp"
#include "core/field_types/core_double_type.hpp"
namespace rive
{
	class StateMachineDoubleBase : public StateMachineInput
	{
	protected:
		typedef StateMachineInput Super;

	public:
		static const uint16_t typeKey = 56;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(uint16_t typeKey) const override
		{
			switch (typeKey)
			{
				case StateMachineDoubleBase::typeKey:
				case StateMachineInputBase::typeKey:
				case StateMachineComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		uint16_t coreType() const override { return typeKey; }

		static const uint16_t valuePropertyKey = 140;

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

		bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case valuePropertyKey:
					m_Value = CoreDoubleType::deserialize(reader);
					return true;
			}
			return StateMachineInput::deserialize(propertyKey, reader);
		}

	protected:
		virtual void valueChanged() {}
	};
} // namespace rive

#endif