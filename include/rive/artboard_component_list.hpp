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
#include "rive/input/focus_node.hpp"
#include "rive/data_bind/data_bind_list_item_consumer.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/symbol_type.hpp"
#include "rive/virtualizing_component.hpp"
#include "rive/refcnt.hpp"
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
class FocusManager;

class ArtboardComponentList final : public ArtboardComponentListBase,
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
#ifdef WITH_RIVE_TOOLS
    /// A mounted item's offset from the list: its layout bounds less the item
    /// artboard's origin.
    Vec2D itemPosition(int index);
#endif
    rcp<ViewModelInstanceListItem> listItem(int index);
    ArtboardInstance* artboardInstance(int index = 0) override;
    /// Logical index of the given instance in the list, or -1 if not found.
    int indexOfArtboardInstance(ArtboardInstance* instance) const;
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
    void hostedRowWoke(Artboard* artboard, uint32_t row) override;
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
    void virtualizableChanged() override;
    void removeVirtualizable(int index) override;
    void setVisibleIndices(int start, int end) override
    {
        m_visibleStartIndex = start;
        m_visibleEndIndex = end;
    }
    void setRealizedIndices(int start, int end) override
    {
        m_realizedStartIndex = start;
        m_realizedEndIndex = end;
        invalidateOrderedListIndicesCache();
    }
    void shouldResetInstances(bool value);
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
    bool isStack();
    LayoutComponent* layoutParent();
    const Mat2D& listTransform() override;
    void listItemTransforms(std::vector<Mat2D*>& transforms) override;
    void addMapRule(ArtboardListMapRule*);
    int type() const override { return coreType(); }
#ifdef WITH_RIVE_EDITOR
    void addMapRuleForEditor(ArtboardListMapRule* rule);
    void removeMapRuleForEditor(ArtboardListMapRule* rule);
#endif

    /// Create/parent a synthetic list scope FocusNode (structural, no
    /// Focusable) so list item focus trees group under it. Idempotent.
    void ensureListScopeFocusNode(FocusManager* focusManager,
                                  rcp<FocusNode> hostParent);
    rcp<FocusNode> listScopeFocusNode() const { return m_listScopeFocusNode; }
    void removeListScopeFocusNode();

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
    bool isWithinVisibleWindow(int index) const;
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

    // Each row's instances, or null for a row that isn't realized. They mirror
    // the maps above, which own the instances, so per-row loops can reach a
    // row without hashing its item.
    std::vector<ArtboardInstance*> m_artboardInstancesByIndex;
    std::vector<StateMachineInstance*> m_stateMachinesByIndex;
    // Points the row at index, and any other row showing the same item, at
    // the item's instances.
    void setRowsForItem(int index,
                        const rcp<ViewModelInstanceListItem>& item,
                        ArtboardInstance* artboard,
                        StateMachineInstance* stateMachine);

    File* m_file = nullptr;
    std::vector<Vec2D> m_artboardSizes;
    Vec2D m_layoutSize;
    int m_visibleStartIndex = -1;
    int m_visibleEndIndex = -1;
    int m_realizedStartIndex = -1;
    int m_realizedEndIndex = -1;
    std::unordered_map<ArtboardInstance*, ArtboardComponentListOverride*>
        m_artboardOverridesMap;
    std::unordered_map<int, int> m_artboardMapRules;

    // Synthetic scope that parents all list item focus subtrees; no Focusable.
    rcp<FocusNode> m_listScopeFocusNode = nullptr;
    // One structural row per list item index, direct child of
    // m_listScopeFocusNode.
    std::vector<rcp<FocusNode>> m_listRowFocusNodes;

    void syncListRowNodesWithList(FocusManager* fm);
    void syncListRowNodesWithList(
        FocusManager* fm,
        const std::vector<rcp<ViewModelInstanceListItem>>& previousListItems,
        const std::vector<rcp<FocusNode>>& previousRowNodes);
    rcp<FocusNode> makeListRowFocusNode() const;
    void reparentListRowsInScope(FocusManager* fm);
    // Whether the scope's children are exactly the rows, in order.
    bool listRowNodesInPlace() const;
    // Wires each realized row's state machine to the manager and builds the
    // row's focus tree under it when needed.
    void buildListRowFocusTrees(FocusManager* fm);
    bool listItemNeedsBuildUnderRow(FocusManager* parentFM,
                                    ArtboardInstance* inst,
                                    rcp<FocusNode> row) const;
    void attachArtboardOverride(ArtboardInstance*,
                                rcp<ViewModelInstanceListItem>);
    void clearArtboardOverride(ArtboardInstance*);
    bool m_shouldResetInstances = false;
    // Whether some item shows on more than one row.
    bool m_listHasDuplicateItems = false;

    // Quiet rows: rows whose per-frame work (their state machine's advance,
    // tryChangeState and updateDataBinds, their artboard's advance, bind
    // updates, reset and update pass) would do nothing are skipped until
    // something wakes them (Artboard::quietHostRow). A bit per row, so a pass
    // steps over 64 quiet rows at a time.
    std::vector<uint64_t> m_quietRows;
    // Rows whose content can't report being quiet, so they aren't checked
    // again every frame.
    std::vector<uint64_t> m_neverQuietRows;
    // The first row at or after `row` that isn't quiet, or the row count.
    size_t nextAwakeRow(size_t row) const;
    bool isRowQuiet(size_t row) const;
    // Marks the row quiet if its work would do nothing; true if it did.
    bool tryQuietRow(size_t row);
    // Whether the row's work would do nothing right now.
    AdvancingComponent::QuietState rowQuietState(size_t row);
    // Wakes the row and forgets it can't be quiet: its instances are about to
    // change.
    void resetQuietRow(size_t row);
    // Wakes every row, then sizes the bits for `rowCount` rows.
    void resetQuietRows(size_t rowCount);
    enum class RowPass
    {
        advance,
        settle,
        updateDataBinds,
        reset,
        update,
    };
#ifdef TESTING
    // Each quiet row's skipped work is still done in tests, after the pass,
    // and must do nothing; a row still quiet must also still be quiet by
    // rowQuietState, or a wake-up was missed.
    void verifyQuietRows(RowPass pass,
                         float elapsedSeconds,
                         AdvanceFlags flags,
                         bool advanceNested);

public:
    // Row passes skipped because the row was quiet, across every list.
    static uint64_t sm_quietRowSkips;
    // Lets a test run the same frames with and without quiet rows.
    static bool sm_quietRowsEnabled;

private:
#endif
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
    // Set while updateList runs, which syncs the focus rows once at its end.
    bool m_updatingList = false;
    /// Always paint / scroll order (ascending drawIndex when enabled).
    std::vector<int> m_cachedOrderedListIndices;
    std::unordered_map<rcp<ViewModelInstanceListItem>,
                       std::unique_ptr<ArtboardListDrawIndexDependent>>
        m_drawIndexDependents;
};
} // namespace rive

#endif