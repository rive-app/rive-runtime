#ifndef _RIVE_STATE_MACHINE_INSTANCE_HPP_
#define _RIVE_STATE_MACHINE_INSTANCE_HPP_

#include <stddef.h>

namespace rive
{
	class StateMachine;
	class StateMachineInputInstance;
	class Artboard;

	class StateMachineLayerInstance;

	class StateMachineInstance
	{
		friend class StateMachineInputInstance;

	private:
		StateMachine* m_Machine;
		bool m_NeedsAdvance = false;

		size_t m_InputCount;
		StateMachineInputInstance** m_InputInstances;
		unsigned int m_LayerCount;
		StateMachineLayerInstance* m_Layers;

		void markNeedsAdvance();

	public:
		StateMachineInstance(StateMachine* machine);
		~StateMachineInstance();

		// Input<bool> findBoolInput(std::string name) const;

		// Advance the state machine by the specified time. Returns true if the
		// state machine will continue to animate after this advance.
		bool advance(float seconds);

		// Applies the animations of the StateMachine to an artboard.
		void apply(Artboard* artboard) const;

		// Returns true when the StateMachineInstance has more data to process.
		bool needsAdvance() const;
	};
} // namespace rive
#endif