#ifndef _RIVE_STATE_MACHINE_LISTENER_HPP_
#define _RIVE_STATE_MACHINE_LISTENER_HPP_
#include "rive/animation/listener_invocation.hpp"
#include "rive/generated/animation/state_machine_listener_base.hpp"
#include "rive/listener_type.hpp"
#include "rive/span.hpp"
#include "rive/animation/listener_types/listener_input_type.hpp"

namespace rive
{
class Shape;
class StateMachineListenerImporter;
class ListenerAction;
class StateMachineInstance;
class StateMachineListener : public StateMachineListenerBase
{
    friend class StateMachineListenerImporter;

public:
    StateMachineListener();
    ~StateMachineListener() override;

    // ListenerType listenerType() const
    // {
    //     return (ListenerType)listenerTypeValue();
    // }
    virtual bool hasListener(ListenerType) const;
    bool hasListeners(Span<const ListenerType> listenerTypes) const;
    // True if any listener type hit-tests a pointer against the target.
    bool hasPointerListeners() const;
    size_t actionCount() const
    {
#ifdef WITH_RIVE_EDITOR
        if (m_actions.empty())
        {
            return m_editorActions.size();
        }
#endif
        return m_actions.size();
    }
    size_t listenerInputTypeCount() const
    {
#ifdef WITH_RIVE_EDITOR
        if (m_listenerInputTypes.empty())
        {
            return m_editorListenerInputTypes.size();
        }
#endif
        return m_listenerInputTypes.size();
    }

    const ListenerAction* action(size_t index) const;
    const ListenerInputType* listenerInputType(size_t index) const;
    StatusCode import(ImportStack& importStack) override;
    // Stamps a targeted LayoutComponent as a listener hit target so the
    // artboard injects its proxy into the draw order (see
    // LayoutComponent::needsDrawableProxy).
    StatusCode onAddedClean(CoreContext* context) override;

    void performChanges(StateMachineInstance* stateMachineInstance,
                        const ListenerInvocation& invocation) const;

#ifdef WITH_RIVE_EDITOR
    // Editor-only parallel non-owning lists. Coop hydrates
    // ListenerAction / ListenerInputType into `EditorFile::m_arena`;
    // these mirror the runtime owning unique_ptr lists. See
    // `StateMachineLayer::m_editorStates` for the pattern rationale.
    void addActionForEditor(ListenerAction* action);
    void addListenerInputTypeForEditor(ListenerInputType* inputType);
    void clearEditorActions();
    void clearEditorListenerInputTypes();
    size_t editorActionCount() const { return m_editorActions.size(); }
    size_t editorListenerInputTypeCount() const
    {
        return m_editorListenerInputTypes.size();
    }
#endif

private:
    void addAction(std::unique_ptr<ListenerAction>);
    void addListenerInputType(std::unique_ptr<ListenerInputType>);
    std::vector<std::unique_ptr<ListenerAction>> m_actions;
    std::vector<std::unique_ptr<ListenerInputType>> m_listenerInputTypes;
#ifdef WITH_RIVE_EDITOR
    std::vector<ListenerAction*> m_editorActions;
    std::vector<ListenerInputType*> m_editorListenerInputTypes;
#endif
};
} // namespace rive

#endif