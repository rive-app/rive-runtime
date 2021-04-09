#include "animation/state_machine_instance.hpp"
#include "animation/state_machine_input.hpp"
#include "animation/state_machine_bool.hpp"
#include "animation/state_machine_double.hpp"
#include "animation/state_machine_trigger.hpp"
#include "animation/state_machine_input_instance.hpp"
#include "animation/state_machine.hpp"
#include "animation/state_machine_layer.hpp"
#include "animation/any_state.hpp"
#include "animation/state_transition.hpp"
#include "animation/transition_condition.hpp"
#include "animation/linear_animation_instance.hpp"
#include "animation/animation_state.hpp"

using namespace rive;

namespace rive
{
	class StateMachineLayerInstance
	{
	private:
		static const int maxIterations = 100;
		const StateMachineLayer* m_Layer = nullptr;
		const LayerState* m_CurrentState = nullptr;
		const LayerState* m_StateFrom = nullptr;
		const StateTransition* m_Transition = nullptr;

		LinearAnimationInstance* m_AnimationInstance = nullptr;
		LinearAnimationInstance* m_AnimationInstanceFrom = nullptr;
		float m_Mix = 1.0f;

	public:
		void init(const StateMachineLayer* layer) { m_Layer = layer; }

		bool advance(float seconds, StateMachineInputInstance** inputs)
		{
			for (int i = 0; updateState(inputs); i++)
			{
				if (i == maxIterations)
				{
					fprintf(stderr, "StateMachine exceeded max iterations.\n");
					return false;
				}
			}
			return true;
		}

		bool updateState(StateMachineInputInstance** inputs)
		{
			if (tryChangeState(m_Layer->anyState(), inputs))
			{
				return true;
			}

			return tryChangeState(m_CurrentState, inputs);
		}

		bool changeState(const LayerState* stateTo)
		{
			if (m_CurrentState == stateTo)
			{
				return false;
			}
			m_CurrentState = stateTo;
			return true;
		}

		bool tryChangeState(const LayerState* stateFrom,
		                    StateMachineInputInstance** inputs)
		{
			if (stateFrom == nullptr)
			{
				return false;
			}
			for (size_t i = 0, length = stateFrom->transitionCount();
			     i < length;
			     i++)
			{
				auto transition = stateFrom->transition(i);
				if (transition->isDisabled())
				{
					continue;
				}

				bool valid = true;
				for (size_t j = 0, length = transition->conditionCount();
				     j < length;
				     j++)
				{
					auto condition = transition->condition(j);
					auto input = inputs[condition->inputId()];
					if (!condition->evaluate(input))
					{
						valid = false;
						break;
					}
				}

				// If all the conditions evaluated to true, make sure the exit
				// time (when set) is also valid.
				if (valid && stateFrom->is<AnimationState>() &&
				    transition->enableExitTime())
				{
					auto fromAnimation =
					    stateFrom->as<AnimationState>()->animation();
					assert(fromAnimation != nullptr);
					auto lastTime = m_AnimationInstance->lastTotalTime();
					auto time = m_AnimationInstance->totalTime();
					auto exitTime =
					    transition->exitTimeSeconds(stateFrom, true);
					if (exitTime < fromAnimation->durationSeconds())
					{
						// Get exit time relative to the loop lastTime was in.
						exitTime +=
						    (int)(lastTime / fromAnimation->durationSeconds()) *
						    fromAnimation->durationSeconds();
					}
					if (time < exitTime)
					{
						valid = false;
					}
				}

				// Try to change the state...
				if (valid && changeState(transition->stateTo()))
				{
					// state actually has changed
					m_Transition = transition;
					m_StateFrom = stateFrom;

					// If we had an exit time and wanted to pause on exit, make
					// sure to hold
					// the exit time.
					if (transition->pauseOnExit() &&
					    transition->enableExitTime() &&
					    m_AnimationInstance != nullptr)
					{
						m_AnimationInstance->time(
						    transition->exitTimeSeconds(stateFrom, false));
					}
				}
			}
			return true;
		}

		void apply(Artboard* artboard) const {}
	};
} // namespace rive

StateMachineInstance::StateMachineInstance(StateMachine* machine) :
    m_Machine(machine)
{
	m_InputCount = machine->inputCount();
	m_InputInstances = new StateMachineInputInstance*[m_InputCount];
	for (int i = 0; i < m_InputCount; i++)
	{
		auto input = machine->input(i);
		if (input == nullptr)
		{
			m_InputInstances[i] = nullptr;
			continue;
		}
		switch (input->coreType())
		{
			case StateMachineBool::typeKey:
				m_InputInstances[i] = new StateMachineBoolInstance(
				    input->as<StateMachineBool>(), this);
				break;
			case StateMachineDouble::typeKey:
				m_InputInstances[i] = new StateMachineNumberInstance(
				    input->as<StateMachineDouble>(), this);
				break;
			case StateMachineTrigger::typeKey:
				m_InputInstances[i] = new StateMachineTriggerInstance(
				    input->as<StateMachineTrigger>(), this);
				break;
			default:
				// Sanity check.
				m_InputInstances[i] = nullptr;
				break;
		}
	}

	m_LayerCount = machine->layerCount();
	for (int i = 0; i < m_LayerCount; i++)
	{
		m_Layers[i].init(machine->layer(i));
	}
}

StateMachineInstance::~StateMachineInstance()
{
	for (int i = 0; i < m_InputCount; i++)
	{
		delete m_InputInstances[i];
	}
	delete[] m_InputInstances;
	delete[] m_Layers;
}

bool StateMachineInstance::updateState() { return false; }

bool StateMachineInstance::advance(float seconds)
{
	m_NeedsAdvance = false;
	for (int i = 0; i < m_LayerCount; i++)
	{
		if (m_Layers[i].advance(seconds, m_InputInstances))
		{
			m_NeedsAdvance = true;
		}
	}

	for (int i = 0; i < m_InputCount; i++)
	{
		m_InputInstances[i]->advanced();
	}

	return m_NeedsAdvance;
}

void StateMachineInstance::apply(Artboard* artboard) const {}

void StateMachineInstance::markNeedsAdvance() { m_NeedsAdvance = true; }
bool StateMachineInstance::needsAdvance() const { return m_NeedsAdvance; }