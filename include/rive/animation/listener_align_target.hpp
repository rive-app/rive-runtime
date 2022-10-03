#ifndef _RIVE_LISTENER_ALIGN_TARGET_HPP_
#define _RIVE_LISTENER_ALIGN_TARGET_HPP_
#include "rive/generated/animation/listener_align_target_base.hpp"
#include <stdio.h>
namespace rive
{
class ListenerAlignTarget : public ListenerAlignTargetBase
{
public:
    void perform(StateMachineInstance* stateMachineInstance, Vec2D position) const override;
};
} // namespace rive

#endif