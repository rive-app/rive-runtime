#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_input.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/any_state.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/animation/transition_condition.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/state_instance.hpp"
#include "rive/animation/animation_state_instance.hpp"

using namespace rive;
namespace rive
{
	class StateMachineLayerInstance
	{
	private:
		static const int maxIterations = 100;
		const StateMachineLayer* m_Layer = nullptr;

		StateInstance* m_AnyStateInstance = nullptr;
		StateInstance* m_CurrentState = nullptr;
		StateInstance* m_StateFrom = nullptr;

		// const LayerState* m_CurrentState = nullptr;
		// const LayerState* m_StateFrom = nullptr;
		const StateTransition* m_Transition = nullptr;

		bool m_HoldAnimationFrom = false;
		// LinearAnimationInstance* m_AnimationInstance = nullptr;
		// LinearAnimationInstance* m_AnimationInstanceFrom = nullptr;
		float m_Mix = 1.0f;
		float m_MixFrom = 1.0f;
		bool m_StateChangedOnAdvance = false;

		bool m_WaitingForExit = false;
		/// Used to ensure a specific animation is applied on the next apply.
		const LinearAnimation* m_HoldAnimation = nullptr;
		float m_HoldTime = 0.0f;

	public:
		~StateMachineLayerInstance()
		{
			delete m_AnyStateInstance;
			delete m_CurrentState;
			delete m_StateFrom;
		}

		void init(const StateMachineLayer* layer)
		{
			assert(m_Layer == nullptr);
			m_AnyStateInstance = layer->anyState()->makeInstance();
			m_Layer = layer;
			changeState(m_Layer->entryState());
		}

		void updateMix(float seconds)
		{
			if (m_Transition != nullptr && m_StateFrom != nullptr &&
			    m_Transition->duration() != 0)
			{
				m_Mix = std::min(
				    1.0f,
				    std::max(0.0f,
				             (m_Mix + seconds / m_Transition->mixTime(
				                                    m_StateFrom->state()))));
			}
			else
			{
				m_Mix = 1.0f;
			}
		}

		bool advance(Artboard* artboard,
		             float seconds,
		             SMIInput** inputs,
		             size_t inputCount)
		{
			m_StateChangedOnAdvance = false;

			if (m_CurrentState != nullptr)
			{
				m_CurrentState->advance(seconds, inputs);
			}

			updateMix(seconds);

			if (m_StateFrom != nullptr && m_Mix < 1.0f && !m_HoldAnimationFrom)
			{
				// This didn't advance during our updateState, but it should now
				// that we realize we need to mix it in.
				m_StateFrom->advance(seconds, inputs);
			}

			for (int i = 0; updateState(inputs, i != 0); i++)
			{
				apply(artboard);

				if (i == maxIterations)
				{
					fprintf(stderr, "StateMachine exceeded max iterations.\n");
					return false;
				}
			}

			apply(artboard);

			return m_Mix != 1.0f || m_WaitingForExit ||
			       (m_CurrentState != nullptr && m_CurrentState->keepGoing());
		}

		bool isTransitioning()
		{
			return m_Transition != nullptr && m_StateFrom != nullptr &&
			       m_Transition->duration() != 0 && m_Mix < 1.0f;
		}

		bool updateState(SMIInput** inputs, bool ignoreTriggers)
		{
			// Don't allow changing state while a transition is taking place
			// (we're mixing one state onto another).
			if (isTransitioning())
			{
				return false;
			}

			m_WaitingForExit = false;

			if (tryChangeState(m_AnyStateInstance, inputs, ignoreTriggers))
			{
				return true;
			}

			return tryChangeState(m_CurrentState, inputs, ignoreTriggers);
		}

		bool changeState(const LayerState* stateTo)
		{
			if ((m_CurrentState == nullptr
			         ? nullptr
			         : m_CurrentState->state()) == stateTo)
			{
				return false;
			}
			m_CurrentState =
			    stateTo == nullptr ? nullptr : stateTo->makeInstance();
			return true;
		}

		bool tryChangeState(StateInstance* stateFromInstance,
		                    SMIInput** inputs,
		                    bool ignoreTriggers)
		{
			if (stateFromInstance == nullptr)
			{
				return false;
			}
			auto stateFrom = stateFromInstance->state();
			auto outState = m_CurrentState;
			for (size_t i = 0, length = stateFrom->transitionCount();
			     i < length;
			     i++)
			{
				auto transition = stateFrom->transition(i);
				auto allowed = transition->allowed(
				    stateFromInstance, inputs, ignoreTriggers);
				if (allowed == AllowTransition::yes &&
				    changeState(transition->stateTo()))
				{
					m_StateChangedOnAdvance = true;
					// state actually has changed
					m_Transition = transition;
					if (m_StateFrom != m_AnyStateInstance)
					{
						// Old state from is done.
						delete m_StateFrom;
					}
					m_StateFrom = outState;

					// If we had an exit time and wanted to pause on exit, make
					// sure to hold the exit time. Delegate this to the
					// transition by telling it that it was completed.
					if (outState != nullptr &&
					    transition->applyExitCondition(outState))
					{
						// Make sure we apply this state. This only returns true
						// when it's an animation state instance.
						auto instance = static_cast<AnimationStateInstance*>(
						                    m_StateFrom)
						                    ->animationInstance();

						m_HoldAnimation = instance->animation();
						m_HoldTime = instance->time();
					}
					m_MixFrom = m_Mix;

					// Keep mixing last animation that was mixed in.
					if (m_Mix != 0.0f)
					{
						m_HoldAnimationFrom = transition->pauseOnExit();
					}
					if (m_StateFrom != nullptr &&
					    m_StateFrom->state()->is<AnimationState>() &&
					    m_CurrentState != nullptr)
					{
						auto instance = static_cast<AnimationStateInstance*>(
						                    m_StateFrom)
						                    ->animationInstance();

						auto spilledTime = instance->spilledTime();
						m_CurrentState->advance(spilledTime, inputs);
					}
					m_Mix = 0.0f;
					updateMix(0.0f);
					m_WaitingForExit = false;
					return true;
				}
				else if (allowed == AllowTransition::waitingForExit)
				{
					m_WaitingForExit = true;
				}
			}
			return false;
		}

		void apply(Artboard* artboard)
		{
			if (m_HoldAnimation != nullptr)
			{
				m_HoldAnimation->apply(artboard, m_HoldTime, m_MixFrom);
				m_HoldAnimation = nullptr;
			}

			if (m_StateFrom != nullptr && m_Mix < 1.0f)
			{
				m_StateFrom->apply(artboard, m_MixFrom);
			}
			if (m_CurrentState != nullptr)
			{
				m_CurrentState->apply(artboard, m_Mix);
			}
		}

		bool stateChangedOnAdvance() const { return m_StateChangedOnAdvance; }

		const LayerState* currentState()
		{
			return m_CurrentState == nullptr ? nullptr
			                                 : m_CurrentState->state();
		}

		const LinearAnimationInstance* currentAnimation() const
		{
			if (m_CurrentState == nullptr ||
			    !m_CurrentState->state()->is<AnimationState>())
			{
				return nullptr;
			}
			return static_cast<AnimationStateInstance*>(m_CurrentState)
			    ->animationInstance();
		}
	};
} // namespace rive

StateMachineInstance::StateMachineInstance(const StateMachine* machine) :
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

bool StateMachineInstance::advance(Artboard* artboard, float seconds)
{
	m_NeedsAdvance = false;
	for (int i = 0; i < m_LayerCount; i++)
	{
		if (m_Layers[i].advance(
		        artboard, seconds, m_InputInstances, m_InputCount))
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

size_t StateMachineInstance::stateChangedCount() const
{
	size_t count = 0;
	for (int i = 0; i < m_LayerCount; i++)
	{
		if (m_Layers[i].stateChangedOnAdvance())
		{
			count++;
		}
	}
	return count;
}

const LayerState* StateMachineInstance::stateChangedByIndex(size_t index) const
{
	size_t count = 0;
	for (int i = 0; i < m_LayerCount; i++)
	{
		if (m_Layers[i].stateChangedOnAdvance())
		{
			if (count == index)
			{
				return m_Layers[i].currentState();
			}
			count++;
		}
	}
	return nullptr;
}

const size_t StateMachineInstance::currentAnimationCount() const
{
	size_t count = 0;
	for (int i = 0; i < m_LayerCount; i++)
	{
		if (m_Layers[i].currentAnimation() != nullptr)
		{
			count++;
		}
	}
	return count;
}

const LinearAnimationInstance*
StateMachineInstance::currentAnimationByIndex(size_t index) const
{
	size_t count = 0;
	for (int i = 0; i < m_LayerCount; i++)
	{
		if (m_Layers[i].currentAnimation() != nullptr)
		{
			if (count == index)
			{
				return m_Layers[i].currentAnimation();
			}
			count++;
		}
	}
	return nullptr;
}