#ifndef _RIVE_STATE_MACHINE_BOOL_BASE_HPP_
#define _RIVE_STATE_MACHINE_BOOL_BASE_HPP_
#include "animation/state_machine_input.hpp"
#include "core/field_types/core_bool_type.hpp"
namespace rive
{
	class StateMachineBoolBase : public StateMachineInput
	{
	protected:
		typedef StateMachineInput Super;

	public:
		static const int typeKey = 59;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case StateMachineBoolBase::typeKey:
				case StateMachineInputBase::typeKey:
				case StateMachineComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int valuePropertyKey = 141;

	private:
		bool m_Value = false;
	public:
		inline bool value() const { return m_Value; }
		void value(bool value)
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
					m_Value = CoreBoolType::deserialize(reader);
					return true;
			}
			return StateMachineInput::deserialize(propertyKey, reader);
		}

	protected:
		virtual void valueChanged() {}
	};
} // namespace rive

#endif