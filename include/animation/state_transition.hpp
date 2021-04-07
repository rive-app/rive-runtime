#ifndef _RIVE_STATE_TRANSITION_HPP_
#define _RIVE_STATE_TRANSITION_HPP_
#include "animation/state_transition_flags.hpp"
#include "generated/animation/state_transition_base.hpp"
#include <stdio.h>
#include <vector>

namespace rive
{
	class LayerState;
	class StateMachineLayerImporter;
	class StateTransitionImporter;
	class TransitionCondition;
	class StateTransition : public StateTransitionBase
	{
		friend class StateMachineLayerImporter;
		friend class StateTransitionImporter;

	private:
		StateTransitionFlags transitionFlags() const
		{
			return static_cast<StateTransitionFlags>(flags());
		}
		LayerState* m_StateTo = nullptr;

		std::vector<TransitionCondition*> m_Conditions;
		void addCondition(TransitionCondition* condition);

	public:
		~StateTransition();
		const LayerState* stateTo() const { return m_StateTo; }

		StatusCode onAddedDirty(CoreContext* context) override;
		StatusCode onAddedClean(CoreContext* context) override;

		bool isDisabled() const
		{
			return (transitionFlags() & StateTransitionFlags::Disabled) ==
			       StateTransitionFlags::Disabled;
		}

		/// Whether the animation is held at exit or if it keeps advancing
		/// during mixing.
		bool pauseOnExit() const
		{
			return (transitionFlags() & StateTransitionFlags::PauseOnExit) !=
			       StateTransitionFlags::PauseOnExit;
		}

		/// Whether exit time is enabled. All other conditions still apply, the
		/// exit time is effectively an AND with the rest of the conditions.
		bool enableExitTime() const
		{
			return (transitionFlags() & StateTransitionFlags::EnableExitTime) !=
			       StateTransitionFlags::EnableExitTime;
		}

		StatusCode import(ImportStack& importStack) override;
#ifdef TESTING
		size_t conditionCount() const { return m_Conditions.size(); }
		TransitionCondition* condition(size_t index) const
		{
			if (index < m_Conditions.size())
			{
				return m_Conditions[index];
			}
			return nullptr;
		}
#endif
	};
} // namespace rive

#endif