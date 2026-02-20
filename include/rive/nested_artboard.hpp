#ifndef _RIVE_NESTED_ARTBOARD_HPP_
#define _RIVE_NESTED_ARTBOARD_HPP_

#include "rive/generated/nested_artboard_base.hpp"
#include "rive/artboard_host.hpp"
#include "rive/artboard_referencer.hpp"
#include "rive/data_bind_path_referencer.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/hit_info.hpp"
#include "rive/span.hpp"
#include "rive/advancing_component.hpp"
#include "rive/resetting_component.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/refcnt.hpp"
#include "rive/file.hpp"
#include <stdio.h>

namespace rive
{

class ArtboardInstance;
class NestedAnimation;
class NestedInput;
class NestedStateMachine;
class StateMachineInstance;
class NestedArtboard : public NestedArtboardBase,
                       public AdvancingComponent,
                       public ResettingComponent,
                       public ArtboardHost,
                       public ArtboardReferencer
{
protected:
    std::unique_ptr<ArtboardInstance> m_Instance; // may be null
    std::unique_ptr<NestedStateMachine>
        m_boundNestedStateMachine; // may be null
    std::vector<NestedAnimation*> m_NestedAnimations;
    File* m_file = nullptr;
    rcp<ViewModelInstance> m_viewModelInstance = nullptr;
    rcp<DataContext> m_dataContext = nullptr;
    // Auto-created ViewModelInstance for stateful artboards.
    rcp<ViewModelInstance> m_statefulViewModelInstance = nullptr;

protected:
private:
    void clearNestedAnimations();
    float m_cumulatedSeconds = 0;
    void nest(Artboard* artboard);

public:
    NestedArtboard();
    ~NestedArtboard() override;
    StatusCode onAddedClean(CoreContext* context) override;
    void draw(Renderer* renderer) override;
    bool willDraw() override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    void addNestedAnimation(NestedAnimation* nestedAnimation);
    void updateArtboard(
        ViewModelInstanceArtboard* viewModelInstanceArtboard) override;
    int referencedArtboardId() override;
    void referencedArtboard(Artboard* artboard) override;
    size_t artboardCount() override { return 1; }
    ArtboardInstance* artboardInstance(int index = 0) override
    {
        return m_Instance.get();
    }
    Artboard* sourceArtboard() { return m_referencedArtboard; }

    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;
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
    void controlSize(Vec2D size,
                     LayoutScaleType widthScaleType,
                     LayoutScaleType heightScaleType,
                     LayoutDirection direction) override;

    /// Convert a world space (relative to the artboard that this
    /// NestedArtboard is a child of) to the local space of the Artboard
    /// nested within. Returns true when the conversion succeeds, and false
    /// when one is not possible.
    bool worldToLocal(Vec2D world, Vec2D* local);
    void decodeDataBindPathIds(Span<const uint8_t> value) override;
    void copyDataBindPathIds(const NestedArtboardBase& object) override;
    void bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance,
                               rcp<DataContext> parent) override;
    void internalDataContext(rcp<DataContext> dataContext) override;
    void clearDataContext() override;
    void unbind() override;
    void updateDataBinds() override;

    float calculateLocalElapsedSeconds(float elapsedSeconds);
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;
    void reset() override;
    Artboard* parentArtboard() override { return artboard(); }
    Vec2D hostTransformPoint(const Vec2D&, ArtboardInstance*) override;
    bool hitTestHost(const Vec2D& position,
                     bool skipOnUnclipped,
                     ArtboardInstance* artboard) override;
    void markHostTransformDirty() override { markTransformDirty(); }
    void file(File*) override;
    File* file() const override;
};
} // namespace rive

#endif
