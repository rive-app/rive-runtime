#ifndef _RIVE_NESTED_ARTBOARD_HPP_
#define _RIVE_NESTED_ARTBOARD_HPP_

#include "rive/generated/nested_artboard_base.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/hit_info.hpp"
#include "rive/span.hpp"
#include <stdio.h>

namespace rive
{

class ArtboardInstance;
class NestedAnimation;
class NestedInput;
class NestedStateMachine;
class StateMachineInstance;
class NestedArtboard : public NestedArtboardBase
{
protected:
    Artboard* m_Artboard = nullptr;               // might point to m_Instance, and might not
    std::unique_ptr<ArtboardInstance> m_Instance; // may be null
    std::vector<NestedAnimation*> m_NestedAnimations;

protected:
    std::vector<uint32_t> m_DataBindPathIdsBuffer;

public:
    NestedArtboard();
    ~NestedArtboard() override;
    StatusCode onAddedClean(CoreContext* context) override;
    void draw(Renderer* renderer) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    void addNestedAnimation(NestedAnimation* nestedAnimation);

    void nest(Artboard* artboard);
    ArtboardInstance* artboardInstance() { return m_Instance.get(); }

    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;
    bool advance(float elapsedSeconds);
    void update(ComponentDirt value) override;

    bool hasNestedStateMachines() const;
    Span<NestedAnimation*> nestedAnimations();
    NestedArtboard* nestedArtboard(std::string name) const;
    NestedStateMachine* stateMachine(std::string name) const;
    NestedInput* input(std::string name) const;
    NestedInput* input(std::string name, std::string stateMachineName) const;

    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
    void controlSize(Vec2D size) override;

    /// Convert a world space (relative to the artboard that this
    /// NestedArtboard is a child of) to the local space of the Artboard
    /// nested within. Returns true when the conversion succeeds, and false
    /// when one is not possible.
    bool worldToLocal(Vec2D world, Vec2D* local);
    void syncStyleChanges();
    void decodeDataBindPathIds(Span<const uint8_t> value) override;
    void copyDataBindPathIds(const NestedArtboardBase& object) override;
    std::vector<uint32_t> dataBindPathIds() { return m_DataBindPathIdsBuffer; };
    void dataContextFromInstance(ViewModelInstance* viewModelInstance, DataContext* parent);
    void internalDataContext(DataContext* dataContext, DataContext* parent);
};
} // namespace rive

#endif