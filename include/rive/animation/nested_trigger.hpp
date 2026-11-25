#ifndef _RIVE_NESTED_TRIGGER_HPP_
#define _RIVE_NESTED_TRIGGER_HPP_
#include "rive/generated/animation/nested_trigger_base.hpp"
#include <stdio.h>
namespace rive
{
class NestedTrigger : public NestedTriggerBase
{
public:
  void applyValue() override;
  void fire(const CallbackData& value) override;
  /// Fires a trigger fired while there was no machine (its host deferred
  /// instancing). NestedStateMachine::initializeAnimation calls it.
  void firePending();
  void resetHeld(const NestedInput* source) override;

private:
  bool m_pendingFire = false;
};
} // namespace rive

#endif