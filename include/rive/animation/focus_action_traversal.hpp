#ifndef _RIVE_FOCUS_ACTION_TRAVERSAL_HPP_
#define _RIVE_FOCUS_ACTION_TRAVERSAL_HPP_
#include "rive/generated/animation/focus_action_traversal_base.hpp"
namespace rive
{
class FocusActionTraversal : public FocusActionTraversalBase
{
public:
    void perform(StateMachineInstance* stateMachineInstance,
                 const ListenerInvocation& invocation) const override;
};
} // namespace rive

#endif