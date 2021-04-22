#include "animation/state_machine_instance.hpp"
#include "animation/state_machine_input.hpp"
#include "animation/state_machine_bool.hpp"
#include "animation/state_machine_number.hpp"
#include "animation/state_machine_trigger.hpp"
#include "animation/state_machine_input_instance.hpp"
#include "animation/state_machine.hpp"
#include "animation/state_machine_layer.hpp"
#include "animation/any_state.hpp"
#include "animation/entry_state.hpp"
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

		bool m_HoldAnimationFrom = false;
		LinearAnimationInstance* m_AnimationInstance = nullptr;
		LinearAnimationInstance* m_AnimationInstanceFrom = nullptr;
		float m_Mix = 1.0f;

	public:
		void init(const StateMachineLayer* layer)
		{
			m_Layer = layer;
			m_CurrentState = m_Layer->entryState();
		}

		bool advance(float seconds, SMIInput** inputs, size_t inputCount)
		{
			bool keepGoing = false;
			if (m_AnimationInstance != nullptr)
			{
				keepGoing = m_AnimationInstance->advance(seconds);
			}

			if (m_Transition != nullptr && m_StateFrom != nullptr &&
			    m_Transition->duration() != 0)
			{
				m_Mix =
				    std::min(1.0f,
				             std::max(0.0f,
				                      (m_Mix + seconds / m_Transition->mixTime(
				                                             m_StateFrom))));
			}
			else
			{
				m_Mix = 1.0f;
			}

			if (m_AnimationInstanceFrom != nullptr && m_Mix < 1.0f &&
			    !m_HoldAnimationFrom)
			{
				m_AnimationInstanceFrom->advance(seconds);
			}

			for (int i = 0; updateState(inputs); i++)
			{
				// Reset inputs between updates.
				for (size_t i = 0; i < inputCount; i++)
				{
					inputs[i]->advanced();
				}

				if (i == maxIterations)
				{
					fprintf(stderr, "StateMachine exceeded max iterations.\n");
					return false;
				}
			}
			return m_Mix != 1.0f || keepGoing;
		}

		bool updateState(SMIInput** inputs)
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

		bool tryChangeState(const LayerState* stateFrom, SMIInput** inputs)
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
					// sure to hold the exit time.
					if (transition->pauseOnExit() &&
					    transition->enableExitTime() &&
					    m_AnimationInstance != nullptr)
					{
						m_AnimationInstance->time(
						    transition->exitTimeSeconds(stateFrom, false));
					}

					if (m_Mix != 0.0f)
					{
						m_HoldAnimationFrom = transition->pauseOnExit();
						delete m_AnimationInstanceFrom;
						m_AnimationInstanceFrom = m_AnimationInstance;

						// Don't let it get deleted as we've passed it on to the
						// from.
						m_AnimationInstance = nullptr;
					}
					else
					{
						delete m_AnimationInstance;
						m_AnimationInstance = nullptr;
					}
					if (m_CurrentState->is<AnimationState>())
					{
						auto animationState =
						    m_CurrentState->as<AnimationState>();
						auto spilledTime =
						    m_AnimationInstanceFrom == nullptr
						        ? 0
						        : m_AnimationInstanceFrom->spilledTime();
						if (animationState->animation() != nullptr)
						{
							m_AnimationInstance = new LinearAnimationInstance(
							    animationState->animation());
							m_AnimationInstance->advance(spilledTime);
						}
						m_Mix = 0.0f;
					}
					return true;
				}
			}
			return false;
		}

		void apply(Artboard* artboard) const
		{
			if (m_AnimationInstanceFrom != nullptr && m_Mix < 1.0f)
			{
				m_AnimationInstanceFrom->animation()->apply(
				    artboard, m_AnimationInstanceFrom->time(), 1.0 - m_Mix);
			}
			if (m_AnimationInstance != nullptr)
			{
				m_AnimationInstance->animation()->apply(
				    artboard, m_AnimationInstance->time(), m_Mix);
			}
		}
	};
} // namespace rive

StateMachineInstance::StateMachineInstance(StateMachine* machine) :
    m_Machine(machine)
{
	m_InputCount = machine->inputCount();
	m_InputInstances = new SMIInput*[m_InputCount];
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
				m_InputInstances[i] =
				    new SMIBool(input->as<StateMachineBool>(), this);
				break;
			case StateMachineNumber::typeKey:
				m_InputInstances[i] =
				    new SMINumber(input->as<StateMachineNumber>(), this);
				break;
			case StateMachineTrigger::typeKey:
				m_InputInstances[i] =
				    new SMITrigger(input->as<StateMachineTrigger>(), this);
				break;
			default:
				// Sanity check.
				m_InputInstances[i] = nullptr;
				break;
		}
	}

	m_LayerCount = machine->layerCount();
	m_Layers = new StateMachineLayerInstance[m_LayerCount];
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

bool StateMachineInstance::advance(float seconds)
{
	m_NeedsAdvance = false;
	for (int i = 0; i < m_LayerCount; i++)
	{
		if (m_Layers[i].advance(seconds, m_InputInstances, m_InputCount))
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

void StateMachineInstance::apply(Artboard* artboard) const
{
	for (int i = 0; i < m_LayerCount; i++)
	{
		m_Layers[i].apply(artboard);
	}
}

void StateMachineInstance::markNeedsAdvance() { m_NeedsAdvance = true; }
bool StateMachineInstance::needsAdvance() const { return m_NeedsAdvance; }

SMIInput* StateMachineInstance::input(size_t index) const
{
	if (index < m_InputCount)
	{
		return m_InputInstances[index];
	}
	return nullptr;
}

SMIBool* StateMachineInstance::getBool(std::string name) const
{
	for (int i = 0; i < m_InputCount; i++)
	{
		auto input = m_InputInstances[i]->input();
		if (input->is<StateMachineBool>() && input->name() == name)
		{
			return static_cast<SMIBool*>(m_InputInstances[i]);
		}
	}
	return nullptr;
}

SMINumber* StateMachineInstance::getNumber(std::string name) const
{
	for (int i = 0; i < m_InputCount; i++)
	{
		auto input = m_InputInstances[i]->input();
		if (input->is<StateMachineNumber>() && input->name() == name)
		{
			return static_cast<SMINumber*>(m_InputInstances[i]);
		}
	}
	return nullptr;
}
SMITrigger* StateMachineInstance::getTrigger(std::string name) const
{
	for (int i = 0; i < m_InputCount; i++)
	{
		auto input = m_InputInstances[i]->input();
		if (input->is<StateMachineTrigger>() && input->name() == name)
		{
			return static_cast<SMITrigger*>(m_InputInstances[i]);
		}
	}
	return nullptr;
}