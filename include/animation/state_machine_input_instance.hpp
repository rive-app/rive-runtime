#ifndef _RIVE_STATE_MACHINE_INPUT_INSTANCE_HPP_
#define _RIVE_STATE_MACHINE_INPUT_INSTANCE_HPP_

namespace rive
{
	class StateMachineInstance;
	class StateMachineInput;
	class StateMachineBool;
	class StateMachineDouble;
	class StateMachineTrigger;

	class StateMachineInputInstance
	{
		friend class StateMachineInstance;

	private:
		StateMachineInstance* m_MachineInstance;
		StateMachineInput* m_Input;

	protected:
		void valueChanged();

		StateMachineInput* input() const { return m_Input; }

		StateMachineInputInstance(StateMachineInput* input,
		                          StateMachineInstance* machineInstance);
	};

	class StateMachineBoolInstance : public StateMachineInputInstance
	{
		friend class StateMachineInstance;

	private:
		bool m_Value;

		StateMachineBoolInstance(StateMachineBool* input,
		                         StateMachineInstance* machineInstance);

	public:
		bool value() const { return m_Value; }
		void value(bool newValue);
	};

	class StateMachineNumberInstance : public StateMachineInputInstance
	{
		friend class StateMachineInstance;

	private:
		float m_Value;

		StateMachineNumberInstance(StateMachineDouble* input,
		                           StateMachineInstance* machineInstance);

	public:
		float value() const { return m_Value; }
		void value(float newValue);
	};

	class StateMachineTriggerInstance : public StateMachineInputInstance
	{
		friend class StateMachineInstance;

	private:
		bool m_Triggered = false;

		StateMachineTriggerInstance(StateMachineTrigger* input,
		                            StateMachineInstance* machineInstance);

	public:
		void fire();
	};
} // namespace rive
#endif