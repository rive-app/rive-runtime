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
#include "rive/viewmodel/symbol_type.hpp"
#include "rive/virtualizing_component.hpp"
#include <memory>
#include <stdio.h>
#include <unordered_map>
#include <vector>
namespace rive
{
class LayoutComponent;
class ScrollConstraint;
class ArtboardListMapRule;
class ArtboardListDrawIndexDependent;

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
    bool collapse(bool value) override;
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
    void internalDataContext(rcp<DataContext> dataContext) override;
    void bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance,
                               rcp<DataContext> parent) override;
    void clearDataContext() override;
    void unbind() override;
    void updateDataBinds() override;
    Artboard* parentArtboard() override { return artboard(); }
    bool hitTestHost(const Vec2D& position,
                     bool skipOnUnclipped,
                     ArtboardInstance* artboard) override;
    Vec2D hostTransformPoint(const Vec2D& vec, ArtboardInstance*) override;
    Mat2D worldTransformForArtboard(ArtboardInstance*) override;
    void markHostTransformDirty() override { markTransformDirty(); }
    Component* hostComponent() override { return this; }
    bool syncStyleChanges() override;
    void updateLayoutBounds(bool animate = true) override;
#ifdef WITH_RIVE_LAYOUT
    bool cascadeLayoutStyle(LayoutStyleInterpolation inheritedInterpolation,
                            KeyFrameInterpolator* inheritedInterpolator,
                            float inheritedInterpolationTime,
                            LayoutDirection direction) override;
#endif
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
        invalidateOrderedListIndicesCache();
    }
    void shouldResetInstances(bool value) { m_shouldResetInstances = value; }
    void setVirtualizablePosition(int index, Vec2D position) override;
    void createArtboardAt(int index, bool forceLayoutSync = true);
    void addArtboardAt(std::unique_ptr<ArtboardInstance> artboard,
                       int index,
                       bool forceLayoutSync = true);
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
    void addMapRule(ArtboardListMapRule*);

    /// Rebuilds the ordered-list cache when invalid (list, visibility, or
    /// drawIndex sort inputs changed).
    void ensureOrderedListIndices();
    /// Paint / scroll order indices; uses drawIndex sorting when any list
    /// item's view model defines SymbolType::drawIndex. Hit-test top-first by
    /// iterating this vector in reverse. Do not retain references across
    /// mutations that invalidate the cache.
    const std::vector<int>& orderedListIndices();
    void invalidateOrderedListIndicesCache();

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
    void createArtboardRecorders(const Artboard*);
    void applyRecorders(Artboard* artboard, const Artboard* sourceArtboard);
    void applyRecorders(StateMachineInstance* stateMachineInstance,
                        const Artboard* sourceArtboard);
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
    std::unordered_map<const Artboard*, std::unique_ptr<PropertyRecorder>>
        m_propertyRecordersMap;
    std::unordered_map<ArtboardInstance*, Mat2D> m_artboardTransforms;
    Vec2D artboardPosition(ArtboardInstance* artboard);

    // Vectors used for access in non-virtualized mode
    std::vector<ArtboardInstance*> m_artboardInstancesByIndex;
    std::vector<StateMachineInstance*> m_stateMachinesByIndex;

    File* m_file = nullptr;
    std::vector<Vec2D> m_artboardSizes;
    Vec2D m_layoutSize;
    int m_visibleStartIndex = -1;
    int m_visibleEndIndex = -1;
    std::unordered_map<ArtboardInstance*, ArtboardComponentListOverride*>
        m_artboardOverridesMap;
    std::unordered_map<int, int> m_artboardMapRules;

    // Data binds that bridge properties between a stateful component's
    // cloned ViewModelInstance and the original (user-provided) one.
    // Keyed by list item so they can be cleaned up when the item is removed.
    std::unordered_map<rcp<ViewModelInstanceListItem>,
                       std::vector<std::unique_ptr<DataBind>>>
        m_bridgeDataBinds;
    void createBridgeBinds(rcp<ViewModelInstanceListItem> listItem,
                           ViewModelInstance* original,
                           ViewModelInstance* clone);
    void removeBridgeBinds(const rcp<ViewModelInstanceListItem>& listItem);
    void attachArtboardOverride(ArtboardInstance*,
                                rcp<ViewModelInstanceListItem>);
    void clearArtboardOverride(ArtboardInstance*);
    bool m_shouldResetInstances = false;
    bool listsAreEqual(std::vector<rcp<ViewModelInstanceListItem>>* list,
                       std::vector<rcp<ViewModelInstanceListItem>>* compared);

    void recomputeListUsesDrawIndexSort();
    float listItemDrawIndex(int index) const;
    void clearDrawIndexListeners();
    void syncDrawIndexListeners();
    void removeDrawIndexListenerForItem(
        const rcp<ViewModelInstanceListItem>& listItem);

    bool m_listUsesDrawIndexSort = false;
    bool m_orderedListIndicesCacheValid = false;
    /// Always paint / scroll order (ascending drawIndex when enabled).
    std::vector<int> m_cachedOrderedListIndices;
    std::unordered_map<rcp<ViewModelInstanceListItem>,
                       std::unique_ptr<ArtboardListDrawIndexDependent>>
        m_drawIndexDependents;
};
} // namespace rive

#endif