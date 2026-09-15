#ifndef _RIVE_SCRIPTED_TRANSITION_HPP_
#define _RIVE_SCRIPTED_TRANSITION_HPP_
#include "rive/generated/scripted/scripted_transition_base.hpp"
#include "rive/data_bind/data_bind_list_item_consumer.hpp"
#include "rive/refcnt.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rive
{
class Component;
class NestedArtboard;
class ArtboardInstance;
class StateMachineInstance;
class File;
class ViewModelInstanceListItem;

/// A ScriptedTransition renders exactly one active child (Solo-style), chosen
/// by activeComponentId, but when the active child changes it hands the
/// outgoing (from) and incoming (to) children to a Transition-protocol script
/// which composites them over time (crossfade / slide / wipe / ...).
///
/// The switchable set is a COMBINED solo set: the design-time NestedArtboard
/// children (indices 0..D-1), followed by artboards instanced from a bound
/// ViewModel list via `listSource` (indices D..D+N-1). List artboards are
/// host-owned (not core objects, so they have no id) and are instanced lazily
/// on first selection. `activeComponentId` data-bound to a number selects by
/// this combined index.
///
/// While a transition is in flight both sources are composited by the script;
/// design children are excluded from the normal draw loop via the Hidden flag,
/// and list instances (which are never in the loop) are drawn by this
/// component.
class ScriptedTransition : public ScriptedTransitionBase,
                           public DataBindListItemConsumer
{
public:
    ~ScriptedTransition() override;

    ScriptProtocol scriptProtocol() override
    {
        return ScriptProtocol::transition;
    }

    void activeComponentIdChanged() override;
    void listSourceChanged() override;
    StatusCode onAddedClean(CoreContext* context) override;
    bool collapse(bool value) override;
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;
    void draw(Renderer* renderer) override;
    bool willDraw() override;
    void update(ComponentDirt value) override;
    Core* clone() const override;

    // File is threaded in by File import (see file.cpp) so list artboards can
    // be resolved from the file's artboard table.
    void file(File* value) { m_file = value; }
    File* file() const { return m_file; }

    // DataBindListItemConsumer: receives the bound ViewModel list (via
    // listSource) and (lazily) instances an artboard per item.
    void updateList(std::vector<rcp<ViewModelInstanceListItem>>* list) override;

    /// The design-time child resolved from activeComponentId (or null).
    Component* activeComponent();

    // Combined-index selectors (design children then list instances).
    void updateByIndex(size_t index);
    void updateByName(const std::string& name);
    int getActiveChildIndex();
    std::string getActiveChildName();

#ifdef WITH_RIVE_LAYOUT
    void recollectOwningLayout();
#endif

private:
    // A renderable source in the combined solo set: either a design-time
    // NestedArtboard child, or a list-instanced ArtboardInstance. Exactly one
    // of the two pointers is set (both null == empty/no selection).
    struct TransitionSource
    {
        NestedArtboard* design = nullptr;
        ArtboardInstance* instance = nullptr;
        bool empty() const { return design == nullptr && instance == nullptr; }
        bool operator==(const TransitionSource& o) const
        {
            return design == o.design && instance == o.instance;
        }
        bool operator!=(const TransitionSource& o) const
        {
            return !(*this == o);
        }
    };

    void propagateCollapse(bool collapse);
    void startTransition(const TransitionSource& from,
                         const TransitionSource& to);
    void completeTransition();
    void callChanged(const TransitionSource& from, const TransitionSource& to);
    bool scriptReady() const;
    bool scriptManagesTo() const;
#ifdef WITH_RIVE_SCRIPTING
    // Resolve a source into the artboard the script draws and the world
    // transform that places it. Design children carry their own transform;
    // list instances are placed at this container's.
    ScriptBackend::TransitionChildRef childRef(
        const TransitionSource& source) const;
#endif

    // Combined solo set + unified selection.
    int designChildCount();
    NestedArtboard* designChildAt(int index);
    // Combined index (design children 0..D-1, then list instances D..) of a
    // source, or -1 if not found. Used to derive the transition direction.
    int sourceIndex(const TransitionSource& source);
    TransitionSource intendedSource();
    void applySelection();
    void selectActiveSource(const TransitionSource& next);

    // list instancing (lazy)
    ArtboardInstance* ensureListInstance(int index);
    Artboard* findListArtboard(const rcp<ViewModelInstanceListItem>& item);
    void disposeListItem(const rcp<ViewModelInstanceListItem>& item);
    void clearListInstances();
    // Drop any selection/transition reference to a list instance that is about
    // to be destroyed, so m_current/m_from/m_to never dangle.
    void forgetInstance(ArtboardInstance* instance);
    void advanceLiveInstance(const TransitionSource& source,
                             float elapsedSeconds,
                             AdvanceFlags flags,
                             bool& keepGoing);

    // The child currently exposed as active (design or list).
    TransitionSource m_current;
    // The in-flight transition pair.
    TransitionSource m_from;
    TransitionSource m_to;
    bool m_transitioning = false;
    // Whether the script composites the incoming (to) source this transition.
    bool m_manageTo = true;
    // >= 0 => active is that list instance; -1 => active is a design child.
    int m_activeListIndex = -1;

    File* m_file = nullptr;
    std::vector<rcp<ViewModelInstanceListItem>> m_listItems;
    std::unordered_map<rcp<ViewModelInstanceListItem>,
                       std::unique_ptr<ArtboardInstance>>
        m_listInstances;
    // Keyed by the instance pointer for O(1) advance lookup. Destroyed before
    // the owning ArtboardInstance (see disposeListItem / clearListInstances).
    std::unordered_map<ArtboardInstance*, std::unique_ptr<StateMachineInstance>>
        m_listStateMachines;
    // Lazily-filled fast index -> instance (nullptr until ensureListInstance).
    std::vector<ArtboardInstance*> m_listInstancesByIndex;
    // Cache: viewModelId -> source artboard in the file.
    std::unordered_map<uint32_t, Artboard*> m_artboardsMap;
};
} // namespace rive

#endif
