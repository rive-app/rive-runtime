#ifndef _RIVE_NESTED_STATE_MACHINE_HPP_
#define _RIVE_NESTED_STATE_MACHINE_HPP_
#include "rive/animation/state_machine_instance.hpp"
#include "rive/generated/animation/nested_state_machine_base.hpp"
#include "rive/math/vec2d.hpp"
#include <memory>

namespace rive
{
class ArtboardInstance;
class NestedInput;
class StateMachineInstance;
class NestedStateMachine : public NestedStateMachineBase
{
private:
    std::unique_ptr<StateMachineInstance> m_StateMachineInstance;
    std::vector<NestedInput*> m_nestedInputs;

public:
    NestedStateMachine();
    ~NestedStateMachine() override;
    void advance(float elapsedSeconds) override;
    void initializeAnimation(ArtboardInstance*) override;
    StateMachineInstance* stateMachineInstance();

    void pointerMove(Vec2D position);
    void pointerDown(Vec2D position);
    void pointerUp(Vec2D position);

    void addNestedInput(NestedInput* input);
};
} // namespace rive

#endif