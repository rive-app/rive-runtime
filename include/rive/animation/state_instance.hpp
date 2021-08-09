#ifndef _RIVE_STATE_INSTANCE_HPP_
#define _RIVE_STATE_INSTANCE_HPP_

#include <string>
#include <stddef.h>

namespace rive
{
	class LayerState;
	class SMIInput;
	class Artboard;

	/// Represents an instance of a state tracked by the State Machine.
	class StateInstance
	{
	private:
		const LayerState* m_LayerState;

	public:
		StateInstance(const LayerState* layerState);
		virtual ~StateInstance();
		virtual void advance(float seconds, SMIInput** inputs) = 0;
		virtual void apply(Artboard* artboard, float mix) = 0;

		/// Returns true when the State Machine needs to keep advancing this
		/// state.
		virtual bool keepGoing() const = 0;

		const LayerState* state() const;
	};
} // namespace rive
#endif