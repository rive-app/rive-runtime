#ifndef _RIVE_ARTBOARD_COMPONENT_LIST_HPP_
#define _RIVE_ARTBOARD_COMPONENT_LIST_HPP_
#include "rive/generated/artboard_component_list_base.hpp"
#include "rive/layout/artboard_component_list_override.hpp"
#include "rive/advancing_component.hpp"
#include "rive/resetting_component.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/constraints/constrainable_list.hpp"
#include "rive/property_recorder.hpp"
#include "rive/file.hpp"
#include "rive/artboard_host.hpp"
#include "rive/data_bind/data_bind_list_item_consumer.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/virtualizing_component.hpp"
#include <stdio.h>
#include <unordered_map>
namespace rive
{
class LayoutComponent;
class ScrollConstraint;

class ArtboardComponentList : public ArtboardComponentListBase,
                              public ArtboardHost,
                              public AdvancingComponent,
                              public ResettingComponent,
                              public LayoutNodeProvider,
                              public DataBindListItemConsumer,
                              public VirtualizingComponent,
                              public ConstrainableList
{
private:
    std::vector<rcp<ViewModelInstanceListItem>> m_listItems;
    std::vector<rcp<ViewModelInstanceListItem>> m_oldItems;

public:
    ArtboardComponentList();
    ~ArtboardComponentList() override;
#ifdef WITH_RIVE_LAYOUT
    void* layoutNode(int index) override;
#endif
    size_t artboardCount() override { return m_listItems.size(); }
    rcp<ViewModelInstanceListItem> listItem(int index);
    ArtboardInstance* artboardInstance(int index = 0) override;
    StateMachineInstance* stateMachineInstance(int index = 0);
    bool worldToLocal(Vec2D world, Vec2D* local, int index);
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;
    void reset() override;
    AABB layoutBounds() override;
    AABB layoutBoundsForNode(int index) override;
    void markHostingLayoutDirty(ArtboardInstance* artboardInstance) override;
    TransformComponent* transformComponent() override
    {
        return this->as<TransformComponent>();
    }
    void updateWorldTransform() override;
    void updateList(std::vector<rcp<ViewModelInstanceListItem>>* list) override;
    void draw(Renderer* renderer) override;
    bool willDraw() override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    void update(ComponentDirt value) override;
    void updateConstraints() override;
    void internalDataContext(DataContext* dataContext) override;
    void bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance,
                               DataContext* parent) override;
    void clearDataContext() override;
    void unbind() override;
    void updateDataBinds() override;
    Artboard* parentArtboard() override { return artboard(); }
    bool hitTestHost(const Vec2D& position,
                     bool skipOnUnclipped,
                     ArtboardInstance* artboard) override;
    Vec2D hostTransformPoint(const Vec2D& vec, ArtboardInstance*) override;
    void markHostTransformDirty() override { markTransformDirty(); }
    bool syncStyleChanges() override;
    void updateLayoutBounds(bool animate = true) override;
    void markLayoutNodeDirty(
        bool shouldForceUpdateLayoutBounds = false) override;
    bool isLayoutProvider() override { return true; }
    size_t numLayoutNodes() override { return m_listItems.size(); }
    void clear();
    void file(File*) override;
    File* file() const override;
    Core* clone() const override;

    // API used by the virtualizer
    Artboard* findArtboard(
        const rcp<ViewModelInstanceListItem>& listItem) const;
    void addVirtualizable(int index) override;
    void removeVirtualizable(int index) override;
    void setVisibleIndices(int start, int end) override
    {
        m_visibleStartIndex = start;
        m_visibleEndIndex = end;
    }
    void shouldResetInstances(bool value) { m_shouldResetInstances = value; }
    void setVirtualizablePosition(int index, Vec2D position) override;
    void createArtboardAt(int index);
    void addArtboardAt(std::unique_ptr<ArtboardInstance> artboard, int index);
    void removeArtboardAt(int index);
    void removeArtboard(rcp<ViewModelInstanceListItem> item);
    bool virtualizationEnabled() override;
    ScrollConstraint* scrollConstraint();
    int itemCount() override { return (int)m_listItems.size(); }
    Virtualizable* item(int index) override { return artboardInstance(index); }
    void setItemSize(Vec2D size, int index) override;
    Vec2D size() override;
    Vec2D itemSize(int index) override;
    float gap();
    void syncLayoutChildren();
    bool mainAxisIsRow();
    LayoutComponent* layoutParent();
    const Mat2D& listTransform() override;
    void listItemTransforms(std::vector<Mat2D*>& transforms) override;

private:
    void updateArtboardsWorldTransform();
    void disposeListItem(const rcp<ViewModelInstanceListItem>& listItem);
    std::unique_ptr<ArtboardInstance> createArtboard(
        Component* target,
        rcp<ViewModelInstanceListItem> listItem) const;
    void bindArtboard(ArtboardInstance* artboard,
                      rcp<ViewModelInstanceListItem> listItem);
    std::unique_ptr<StateMachineInstance> createStateMachineInstance(
        Component* target,
        ArtboardInstance* artboard);
    void linkStateMachineToArtboard(StateMachineInstance* stateMachineInstance,
                                    ArtboardInstance* artboard);
    void computeLayoutBounds();
    void createArtboardRecorders(Artboard*);
    void applyRecorders(Artboard* artboard, Artboard* sourceArtboard);
    void applyRecorders(StateMachineInstance* stateMachineInstance,
                        Artboard* sourceArtboard);
    mutable std::unordered_map<uint32_t, Artboard*> m_artboardsMap;
    std::unordered_map<rcp<ViewModelInstanceListItem>,
                       std::unique_ptr<ArtboardInstance>>
        m_artboardInstancesMap;
    std::unordered_map<rcp<ViewModelInstanceListItem>,
                       std::unique_ptr<StateMachineInstance>>
        m_stateMachinesMap;
    std::unordered_map<Artboard*,
                       std::vector<std::unique_ptr<ArtboardInstance>>>
        m_resourcePool;
    std::unordered_map<Artboard*,
                       std::vector<std::unique_ptr<StateMachineInstance>>>
        m_stateMachinesPool;
    std::unordered_map<Artboard*, std::unique_ptr<PropertyRecorder>>
        m_propertyRecordersMap;
    std::unordered_map<ArtboardInstance*, Mat2D> m_artboardTransforms;
    Vec2D artboardPosition(ArtboardInstance* artboard);

    File* m_file = nullptr;
    std::vector<Vec2D> m_artboardSizes;
    Vec2D m_layoutSize;
    int m_visibleStartIndex = -1;
    int m_visibleEndIndex = -1;
    std::unordered_map<ArtboardInstance*, ArtboardComponentListOverride*>
        m_artboardOverridesMap;
    void attachArtboardOverride(ArtboardInstance*,
                                rcp<ViewModelInstanceListItem>);
    void clearArtboardOverride(ArtboardInstance*);
    bool m_shouldResetInstances = false;
    bool listsAreEqual(std::vector<rcp<ViewModelInstanceListItem>>* list,
                       std::vector<rcp<ViewModelInstanceListItem>>* compared);
};
} // namespace rive

#endif