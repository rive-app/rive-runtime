#ifndef _RIVE_ARTBOARD_COMPONENT_LIST_HPP_
#define _RIVE_ARTBOARD_COMPONENT_LIST_HPP_
#include "rive/generated/artboard_component_list_base.hpp"
#include "rive/advancing_component.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_host.hpp"
#include "rive/data_bind/data_bind_list_item_consumer.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include <stdio.h>
#include <unordered_map>
namespace rive
{
class File;

class ArtboardComponentList : public ArtboardComponentListBase,
                              public ArtboardHost,
                              public AdvancingComponent,
                              public LayoutNodeProvider,
                              public DataBindListItemConsumer
{
private:
    std::vector<ViewModelInstanceListItem*> m_listItems;

public:
    ArtboardComponentList();
    ~ArtboardComponentList() override;
#ifdef WITH_RIVE_LAYOUT
    void* layoutNode(int index) override;
#endif
    size_t artboardCount() override { return m_listItems.size(); }
    ViewModelInstanceListItem* listItem(int index)
    {
        if (index < m_listItems.size())
        {
            return m_listItems[index];
        }
        return nullptr;
    }
    ArtboardInstance* artboardInstance(int index = 0) override
    {
        if (index < m_listItems.size())
        {
            return m_artboardInstancesMap[listItem(index)].get();
        }
        return nullptr;
    }
    StateMachineInstance* stateMachineInstance(int index = 0)
    {
        if (index < m_listItems.size())
        {
            return m_stateMachinesMap[listItem(index)].get();
        }
        return nullptr;
    }
    bool worldToLocal(Vec2D world, Vec2D* local, int index);
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;
    AABB layoutBounds() override;
    AABB layoutBoundsForNode(int index) override;
    void markHostingLayoutDirty(ArtboardInstance* artboardInstance) override;
    TransformComponent* transformComponent() override
    {
        return this->as<TransformComponent>();
    }
    void updateList(int propertyKey,
                    std::vector<ViewModelInstanceListItem*>* list) override;
    void draw(Renderer* renderer) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    void update(ComponentDirt value) override;
    void updateConstraints() override;
    void internalDataContext(DataContext* dataContext) override;
    void bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance,
                               DataContext* parent) override;
    void clearDataContext() override;
    void updateDataBinds() override;
    Artboard* parentArtboard() override { return artboard(); }
    void markHostTransformDirty() override { markTransformDirty(); }
    bool syncStyleChanges() override;
    void updateLayoutBounds(bool animate = true) override;
    void markLayoutNodeDirty(
        bool shouldForceUpdateLayoutBounds = false) override;
    bool isLayoutProvider() override { return true; }
    size_t numLayoutNodes() override { return m_listItems.size(); }
    void reset();
    void file(File*);
    File* file() const;
    Core* clone() const override;

private:
    void disposeListItem(ViewModelInstanceListItem* listItem);
    std::unique_ptr<ArtboardInstance> createArtboard(
        Component* target,
        ViewModelInstanceListItem* listItem) const;
    Artboard* findArtboard(ViewModelInstanceListItem* listItem) const;
    std::unique_ptr<StateMachineInstance> createStateMachineInstance(
        Component* target,
        ArtboardInstance* artboard);
    mutable std::unordered_map<uint32_t, Artboard*> m_artboardsMap;
    std::unordered_map<ViewModelInstanceListItem*,
                       std::unique_ptr<ArtboardInstance>>
        m_artboardInstancesMap;
    std::unordered_map<ViewModelInstanceListItem*,
                       std::unique_ptr<StateMachineInstance>>
        m_stateMachinesMap;
    File* m_file = nullptr;
};
} // namespace rive

#endif