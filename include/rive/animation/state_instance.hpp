#ifndef _RIVE_STATE_INSTANCE_HPP_
#define _RIVE_STATE_INSTANCE_HPP_

#include <string>
#include <stddef.h>
#include <functional>
#include "rive/rive_types.hpp"
#include "rive/span.hpp"

namespace rive
{
class LayerState;
class StateMachineInstance;
class ArtboardInstance;
class LinearAnimationInstance;

/// Represents an instance of a state tracked by the State Machine.
class StateInstance
{
private:
    const LayerState* m_LayerState;

public:
    StateInstance(const LayerState* layerState);
    virtual ~StateInstance();
    virtual void advance(float seconds,
                         StateMachineInstance* stateMachineInstance) = 0;
    virtual void apply(ArtboardInstance* instance, float mix) = 0;

    /// Returns true when the State Machine needs to keep advancing this
    /// state.
    virtual bool keepGoing() const = 0;
    virtual void clearSpilledTime() {}

    /// Invokes [callback] for each LinearAnimationInstance this state drives.
    /// Used by StateMachineInstance to build/tear down keyframe value data
    /// binds over the lifetime of the state instance. Default is a no-op for
    /// states with no animation (entry/exit/any).
    virtual void forEachAnimationInstance(
        const std::function<void(LinearAnimationInstance*)>& callback)
    {}

    const LayerState* state() const;
};
} // namespace rive
#endif