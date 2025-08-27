#ifndef _RIVE_NESTED_STATE_MACHINE_HPP_
#define _RIVE_NESTED_STATE_MACHINE_HPP_
#include "rive/animation/state_machine_instance.hpp"
#include "rive/generated/animation/nested_state_machine_base.hpp"
#include "rive/hit_result.hpp"
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
    bool advance(float elapsedSeconds, bool newFrame) override;
    void initializeAnimation(ArtboardInstance*) override;
    StateMachineInstance* stateMachineInstance();

    HitResult pointerMove(Vec2D position, float timeStamp = 0);
    HitResult pointerDown(Vec2D position);
    HitResult pointerUp(Vec2D position);
    HitResult pointerExit(Vec2D position);
    HitResult dragStart(Vec2D position, float timeStamp = 0);
    HitResult dragEnd(Vec2D position, float timeStamp = 0);
    bool tryChangeState();
    bool hitTest(Vec2D position) const;

    void addNestedInput(NestedInput* input);
    size_t inputCount() { return m_nestedInputs.size(); }
    NestedInput* input(size_t index);
    NestedInput* input(std::string name);
    void bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance);
    void dataContext(DataContext* dataContext);
    void releaseDependencies() override
    {
        m_StateMachineInstance.reset(nullptr);
    }
    void clearDataContext();
};
} // namespace rive

#endif