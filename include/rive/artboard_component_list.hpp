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
#include "rive/lazy_vector.hpp"
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

public:
#ifdef WITH_RIVE_EDITOR
  void editorPrepareForDestruction() override;
  // The pooled rows too: they keep the list as their host.
  void forEachHostedArtboard(
      const std::function<void(ArtboardInstance*)>& visit) override;
#endif
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
  /// Where a pointer last was, in the space worldToLocal maps from, as the
  /// host passes it on to the rows. Forgotten once it exits.
  void notePointer(int pointerId, Vec2D position);
  void forgetPointer(int pointerId);
  bool collapse(bool value) override;
  bool advanceComponent(
      float elapsedSeconds,
      AdvanceFlags flags = AdvanceFlags::Animate |
                           AdvanceFlags::NewFrame) override;
  // Quiet once every row is: a list inside a list row then lets that row go
  // quiet too.
  QuietState quietState() override;
  // Whether every row is quiet, so a change to one wakes the row this list
  // is on (see Artboard::hostedSkippableWhileCollapsed).
  bool allRowsQuiet() const;
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
  void boundListSource(const Core* source) override { m_listSource = source; }
  const Core* hostedRowsSource() const override { return m_listSource; }

  // How deep rows may nest while one is built: building a row binds it, and
  // the lists inside it build their rows right then, on the same stack. A
  // row that ends up hosting rows of itself would otherwise recurse until
  // the stack overflows (RIVE_NATIVE-18W). Lower where files are authored,
  // so a runaway nesting stops before it has built many rows.
#ifdef WITH_RIVE_TOOLS
  static constexpr uint32_t maxRowNesting = 16;
#else
  static constexpr uint32_t maxRowNesting = 64;
#endif

  void draw(Renderer* renderer) override;
  bool willDraw() override;
  Core* hitTest(HitInfo*, const Mat2D&) override;
  void hostedRowWoke(Artboard* artboard, uint32_t row) override;
  void update(ComponentDirt value) override;
  void updateConstraints() override;
  void internalDataContext(rcp<DataContext> dataContext) override;
  void bindViewModelInstance(
      rcp<ViewModelInstance> viewModelInstance,
      rcp<DataContext> parent) override;
  void clearDataContext() override;
  void unbind() override;
  void updateDataBinds() override;
  Artboard* parentArtboard() override { return artboard(); }
  bool hitTestHost(
      const Vec2D& position,
      bool skipOnUnclipped,
      ArtboardInstance* artboard) override;
  Vec2D hostTransformPoint(const Vec2D& vec, ArtboardInstance*) override;
  Mat2D worldTransformForArtboard(ArtboardInstance*) override;
  void markHostTransformDirty() override { markTransformDirty(); }
  Component* hostComponent() override { return this; }
  bool syncStyleChanges() override;
  void updateLayoutBounds(bool animate = true) override;
#ifdef WITH_RIVE_LAYOUT
  bool cascadeLayoutStyle(
      LayoutStyleInterpolation inheritedInterpolation,
      KeyFrameInterpolator* inheritedInterpolator,
      float inheritedInterpolationTime,
      LayoutDirection direction) override;
#endif
  void markLayoutNodeDirty(bool shouldForceUpdateLayoutBounds = false) override;
  bool isLayoutProvider() override { return true; }
  // updateList syncs once all its rows are in, and a row realized on its own
  // syncs once it's findable by its index.
  bool syncsLayoutChildren() override { return true; }
  size_t numLayoutNodes() override { return m_listItems.size(); }
#ifdef WITH_RIVE_LAYOUT
  void collectLayoutNodes(std::vector<void*>& out) override;
#endif
  void clear();
  void file(File*) override;
  File* file() const override;
  Core* clone() const override;

  // API used by the virtualizer
  Artboard* findArtboard(const rcp<ViewModelInstanceListItem>& listItem) const;
  void addVirtualizable(int index) override;
  void virtualizableChanged() override;
  void removeVirtualizable(int index) override;
  void realizedIndices(std::vector<int>& out) override;
  uint32_t itemsVersion() override { return m_itemsVersion; }
  void clearVirtualWindow() override;
  void addToVirtualWindow(int index, bool visible) override;
  void setVirtualizablePosition(int index, Vec2D position) override;
  void setVirtualizableCell(int index, int column, int row) override;
  void createArtboardAt(int index, bool forceLayoutSync = true);
  // Hosts artboard as the row of the item at index. A pooled row comes with
  // the state machine it ran, which is reset rather than made again.
  void addArtboardAt(
      std::unique_ptr<ArtboardInstance> artboard,
      int index,
      bool forceLayoutSync = true,
      std::unique_ptr<StateMachineInstance> stateMachine = nullptr);
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
  void ensureListScopeFocusNode(
      FocusManager* focusManager,
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
  // Moves a removed item's row into the pools so an item added in the same
  // update can take it instead of instancing a new one.
  void poolListItem(const rcp<ViewModelInstanceListItem>& listItem);
  // Builds the row for the item at index, from the pool when one is there.
  void createOrReuseArtboardAt(int index);
  // True when a row of source, built now, would nest in itself: past
  // maxRowNesting, or inside a row of the same artboard that a list made
  // from the same source as this one, whose rows would repeat it forever.
  bool refusesNestedRow(const Artboard* source) const;
  // Destroys the pooled rows, state machines first.
  void releasePools();
  // Takes every row out of the list into the pools, as an update to no items
  // would, but keeps them there for the next update to take.
  void restartRows();
  std::unique_ptr<ArtboardInstance> createArtboard(
      Component* target,
      rcp<ViewModelInstanceListItem> listItem) const;
  void bindArtboard(
      ArtboardInstance* artboard,
      rcp<ViewModelInstanceListItem> listItem);
  std::unique_ptr<StateMachineInstance> createStateMachineInstance(
      Component* target,
      ArtboardInstance* artboard);
  void linkStateMachineToArtboard(
      StateMachineInstance* stateMachineInstance,
      ArtboardInstance* artboard);
  void computeLayoutBounds();
  bool isWithinVisibleWindow(int index) const;
  bool viewportCullsBufferedRows(
      Renderer* renderer,
      const Mat2D& listTransform,
      Mat2D* listToDevice,
      AABB* deviceViewport);
  void createArtboardRecorders(const Artboard*);
  void applyRecorders(Artboard* artboard, const Artboard* sourceArtboard);
  void applyRecorders(
      StateMachineInstance* stateMachineInstance,
      const Artboard* sourceArtboard);
  mutable std::unordered_map<uint32_t, Artboard*> m_artboardsMap;
  std::unordered_map<
      rcp<ViewModelInstanceListItem>,
      std::unique_ptr<ArtboardInstance>>
      m_artboardInstancesMap;
  std::unordered_map<
      rcp<ViewModelInstanceListItem>,
      std::unique_ptr<StateMachineInstance>>
      m_stateMachinesMap;
  // A row out of the list, for another item to take, with the state machine
  // it ran. Kept together: the machine was built on this artboard's objects
  // and reads them until it is destroyed, which happens first (members go in
  // reverse order).
  struct PooledRow
  {
    std::unique_ptr<ArtboardInstance> artboard;
    std::unique_ptr<StateMachineInstance> stateMachine;
  };
  // Keyed by the artboard the rows are instances of.
  std::unordered_map<Artboard*, std::vector<PooledRow>> m_rowPool;
  std::unordered_map<const Artboard*, std::unique_ptr<PropertyRecorder>>
      m_propertyRecordersMap;
  std::unordered_map<ArtboardInstance*, Mat2D> m_artboardTransforms;
  Vec2D artboardPosition(ArtboardInstance* artboard);

  // Each row's instances, or null for a row that isn't realized. They mirror
  // the maps above, which own the instances, so per-row loops can reach a
  // row without hashing its item.
  std::vector<ArtboardInstance*> m_artboardInstancesByIndex;
  // Sets a row's instance, and tells the instance its row.
  void placeInRow(size_t index, ArtboardInstance* artboard);
  // Whether a realized row holds artboard.
  bool showsInARow(ArtboardInstance* artboard);
  std::vector<StateMachineInstance*> m_stateMachinesByIndex;
  // Points the row at index, and any other row showing the same item, at
  // the item's instances.
  void setRowsForItem(
      int index,
      const rcp<ViewModelInstanceListItem>& item,
      ArtboardInstance* artboard,
      StateMachineInstance* stateMachine);

  File* m_file = nullptr;
  std::vector<Vec2D> m_artboardSizes;
  // Bumped when the items or their sizes change.
  uint32_t m_itemsVersion = 0;
  // Rows holding an artboard, sorted; per-row loops visit only these.
  std::vector<int> m_realizedIndices;
  void markRealized(size_t index, bool realized);
  // A copy, so the callback may realize or recycle rows.
  template <typename F> void forEachRealized(F callback)
  {
    // Indexed, so a callback that realizes a row can't invalidate it.
    for (size_t k = 0; k < m_realizedIndices.size(); k++)
    {
      int i = m_realizedIndices[k];
      if (auto artboard = m_artboardInstancesByIndex[i])
      {
        callback(i, artboard);
      }
    }
  }
  Vec2D m_layoutSize;
  // What m_layoutSize was summed from.
  uint32_t m_layoutSizeVersion = 0;
  float m_layoutSizeGap = 0.0f;
  bool m_layoutSizeIsHorz = false;
  // Realized items in walk order, and each item's place in the window.
  std::vector<int> m_windowOrder;
  std::vector<uint8_t> m_windowState;
  int m_windowVisibleCount = 0;
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
  bool listItemNeedsBuildUnderRow(
      FocusManager* parentFM,
      ArtboardInstance* inst,
      rcp<FocusNode> row) const;
  void attachArtboardOverride(ArtboardInstance*);
  void clearArtboardOverride(ArtboardInstance*);
  // Whether some item shows on more than one row.
  bool m_listHasDuplicateItems = false;

  // The pointers notePointer has seen and that haven't exited yet.
  struct NotedPointer
  {
    int id;
    Vec2D position;
  };
  LazyVector<NotedPointer> m_notedPointers;
  // A row the list stops showing doesn't see the pointer leave, as the list
  // passes it no more events. It gets every noted pointer's exit here
  // instead, while it is still bound to the item its exit listeners write.
  void exitNotedPointers(int index);
  // The same for the rows of the items an update to list removes, while
  // they are still where they were.
  void exitRowsLeaving(const std::vector<rcp<ViewModelInstanceListItem>>& list);

  // Quiet rows: rows whose per-frame work (their state machine's advance,
  // tryChangeState and updateDataBinds, their artboard's advance, bind
  // updates and update pass) would do nothing are skipped until something
  // wakes them (Artboard::quietHostRow). A bit per row, so a pass steps over
  // 64 quiet rows at a time.
  std::vector<uint64_t> m_quietRows;
  // Rows whose content can't report being quiet, so they aren't checked
  // again every frame.
  std::vector<uint64_t> m_neverQuietRows;
  // The first row at or after `row` that isn't quiet, or the row count.
  size_t nextAwakeRow(size_t row) const;
  bool isRowQuiet(size_t row) const;
  bool canQuietRows() const;
  // Marks the row quiet if its work would do nothing; true if it did.
  bool tryQuietRow(size_t row);
  // Quiets every row while none are realized; realizing a row wakes it.
  void quietUnrealizedRows();
  // Wakes every quiet row, for a change none of them can see coming.
  void wakeQuietRows();
  // TextLanguages::generation() the rows last advanced under: text in a
  // quiet row inherits the system languages too, so a change wakes them.
  uint32_t m_languageGeneration = 0;
#ifdef WITH_RIVE_TEXT
  // Font::fallbackFontsGeneration() the rows last advanced under, for the
  // same reason: text in a quiet row reshapes for new fallback fonts too.
  uint32_t m_fallbackFontsGeneration = 0;
#endif
  // Whether the system languages or fallback fonts changed since the rows
  // last advanced.
  bool textEnvironmentChanged() const;
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
    update,
  };
#ifdef TESTING
  // Each quiet row's skipped work is still done in tests, after the pass,
  // and must do nothing; a row still quiet must also still be quiet by
  // rowQuietState, or a wake-up was missed.
  void verifyQuietRows(
      RowPass pass,
      float elapsedSeconds,
      AdvanceFlags flags,
      bool advanceNested);

public:
  // Row passes skipped because the row was quiet, across every list.
  static uint64_t sm_quietRowSkips;
  // Rows scanned for an artboard because the list repeats items.
  static uint32_t sm_rowScans;
  // Lets a test run the same frames with and without quiet rows.
  static bool sm_quietRowsEnabled;
  // Whether a row that its machine's advance leaves quiet goes quiet then,
  // skipping its artboard's advance, instead of at the next frame's start.
  // On by default.
  static bool sm_requietAfterMachine;
  // Lets a benchmark time quiet rows without checking each one every pass.
  static bool sm_verifyQuietRows;
  // Lets a test run the same frames with and without handing rows that host
  // artboards to new items (see poolListItem).
  static bool sm_reuseHostingRows;
  // Rows hosting artboards that a list handed to a new item.
  static uint64_t sm_reusedHostingRows;
  // Set while the verifier does a quiet row's skipped work: a collapsed or
  // paused host's advance then notes nothing, as the pass never ran.
  static bool sm_verifyingQuietRow;

private:
#endif
  bool listsAreEqual(
      std::vector<rcp<ViewModelInstanceListItem>>* list,
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
  // Whether the last advance ran the rows' state machines (AdvanceNested).
  // updateList only enters a new row's initial state early when it did.
  bool m_advancesNested = true;
  // Set while restartRows runs: the rows it pools stay for the next update.
  bool m_keepPools = false;
  // Whether the rows were told the list is fully transparent, through their
  // host opacity. Any other opacity applies when the list draws them.
  bool m_rowsHidden = false;
  // What the bound list comes from (see boundListSource); null when unbound
  // or when a converter makes the list.
  const Core* m_listSource = nullptr;
  /// Always paint / scroll order (ascending drawIndex when enabled).
  std::vector<int> m_cachedOrderedListIndices;
  std::unordered_map<
      rcp<ViewModelInstanceListItem>,
      std::unique_ptr<ArtboardListDrawIndexDependent>>
      m_drawIndexDependents;
};
} // namespace rive

#endif