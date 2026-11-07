#ifndef _RIVE_STATE_INSTANCE_HPP_
#define _RIVE_STATE_INSTANCE_HPP_

#include <string>
#include <stddef.h>
#include "rive/rive_types.hpp"
#include "rive/span.hpp"

namespace rive
{
class BlendAccumulator;
class LayerState;
class StateMachineInstance;
class ArtboardInstance;

/// Represents an instance of a state tracked by the State Machine.
class StateInstance
{
private:
  const LayerState* m_LayerState;

public:
  StateInstance(const LayerState* layerState);
  virtual ~StateInstance();
  virtual void advance(
      float seconds,
      StateMachineInstance* stateMachineInstance) = 0;
  virtual void apply(ArtboardInstance* instance, float mix) = 0;

  /// Returns true when the State Machine needs to keep advancing this
  /// state.
  virtual bool keepGoing() const = 0;
  virtual void clearSpilledTime() {}

  /// The state machine's accumulator for the properties several of its
  /// layers key (see StateMachineInstance): what this state applies to
  /// those properties goes to it.
  virtual void layerAccumulator(BlendAccumulator* accumulator) {}
  /// Starts the state over, as a new instance of it would, so a layer going
  /// back to it can reuse this one. False if it can't: the layer then makes
  /// a new one.
  virtual bool restart() { return false; }

  const LayerState* state() const;
};
} // namespace rive
#endif