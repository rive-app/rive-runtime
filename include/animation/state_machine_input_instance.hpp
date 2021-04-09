#ifndef _RIVE_STATE_MACHINE_INPUT_INSTANCE_HPP_
#define _RIVE_STATE_MACHINE_INPUT_INSTANCE_HPP_

namespace rive
{
	class StateMachineInstance;
	class StateMachineInput;
	class StateMachineBool;
	class StateMachineDouble;
	class StateMachineTrigger;
	class TransitionTriggerCondition;

	class StateMachineInputInstance
	{
		friend class StateMachineInstance;

	private:
		StateMachineInstance* m_MachineInstance;
		const StateMachineInput* m_Input;

		virtual void advanced() {}

	protected:
		void valueChanged();

		StateMachineInputInstance(const StateMachineInput* input,
		                          StateMachineInstance* machineInstance);
		virtual ~StateMachineInputInstance() {}

	public:
		const StateMachineInput* input() const { return m_Input; }
	};

	class StateMachineBoolInstance : public StateMachineInputInstance
	{
		friend class StateMachineInstance;

	private:
		bool m_Value;

		StateMachineBoolInstance(const StateMachineBool* input,
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

		StateMachineNumberInstance(const StateMachineDouble* input,
		                           StateMachineInstance* machineInstance);

	public:
		float value() const { return m_Value; }
		void value(float newValue);
	};

	class StateMachineTriggerInstance : public StateMachineInputInstance
	{
		friend class StateMachineInstance;
		friend class TransitionTriggerCondition;

	private:
		bool m_Fired = false;

		StateMachineTriggerInstance(const StateMachineTrigger* input,
		                            StateMachineInstance* machineInstance);
		void advanced() override { m_Fired = false; }

	public:
		void fire();
	};
} // namespace rive
#endif