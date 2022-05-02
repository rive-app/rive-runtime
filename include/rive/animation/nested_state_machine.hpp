#ifndef _RIVE_NESTED_STATE_MACHINE_HPP_
#define _RIVE_NESTED_STATE_MACHINE_HPP_
#include "rive/generated/animation/nested_state_machine_base.hpp"
#include <stdio.h>
namespace rive {
    class ArtboardInstance;

    class NestedStateMachine : public NestedStateMachineBase {
    public:
        void advance(float elapsedSeconds) override;
        void initializeAnimation(ArtboardInstance*) override;
    };
} // namespace rive

#endif