#ifndef _RIVE_VIEW_MODEL_INSTANCE_VALUE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_VALUE_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_value_base.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
#include "rive/dependency_helper.hpp"
#include "rive/lazy_vector.hpp"
#include "rive/viewmodel/viewmodel_value_dependent.hpp"
#include "rive/component.hpp"
#include "rive/component_dirt.hpp"
#include "rive/refcnt.hpp"
#include <stdio.h>
#include <atomic>
#include <cstdint>
#include <list>

namespace rive
{
class ViewModelInstance;
class ViewModelInstanceValueDelegate
{
public:
    virtual void valueChanged() = 0;
};

enum class ValueFlags : uint8_t
{
    none = 0,
    delegatesChanged = 1 << 2,
    delegating = 1 << 3,
};

class SuppressDelegation;

class ViewModelInstanceValue : public ViewModelInstanceValueBase,
                               public RefCnt<ViewModelInstanceValue>
{
    friend class SuppressDelegation;

private:
    ViewModelProperty* m_ViewModelProperty = nullptr;
    static std::string defaultName;
    static std::atomic<uint64_t> s_changeSequence;
    ValueFlags m_changeFlags = ValueFlags::none;
    uint64_t m_changeSequence = 0;
    LazyVector<ViewModelInstanceValueDelegate*> m_delegates;
    LazyVector<ViewModelInstanceValueDelegate*> m_delegatesCopy;
    void registerSymbol();

public:
    void addDelegate(ViewModelInstanceValueDelegate* delegate);
    void removeDelegate(ViewModelInstanceValueDelegate* delegate);

    // Required by DependencyHelper's CRTP base. onComponentDirty is never
    // actually invoked on ViewModelInstanceValue's dependency chain — the
    // previous design stored &local_param here (a dangling pointer) and was
    // saved only by nothing reading it. We keep a stub returning nullptr; if
    // anything ever calls it, the deref will crash loudly rather than
    // silently use freed stack memory. The stub also satisfies the CRTP
    // contract so the templated onComponentDirty member can instantiate.
    rcp<ViewModelInstance>* dependencyRoot() const { return nullptr; }

protected:
    // Kept as a member rather than a base because ViewModelInstanceValue
    // already inherits DependencyHelper transitively via Component (see
    // ViewModelInstanceValueBase). Inheriting a second DependencyHelper
    // would make `addDependent` ambiguous across the two instantiations.
    // The 8 B savings from the CRTP refactor still apply — the helper no
    // longer stores its own root pointer.
    DependencyHelper<rcp<ViewModelInstance>,
                     ViewModelValueDependent,
                     ViewModelInstanceValue>
        m_DependencyHelper;
    ViewModelInstance* m_viewModelInstance = nullptr;
    void addDirt(ComponentDirt value);

    // Suppress/restore calling delegates.
    bool suppressDelegation();
    void restoreDelegation();

public:
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode import(ImportStack& importStack) override;
    void viewModelProperty(ViewModelProperty* value);
    ViewModelProperty* viewModelProperty();
    void viewModelInstance(ViewModelInstance* value);
    // Not tools-only: a scripted property wrapper keeps the instance that owns
    // its value alive through this, in every build.
    ViewModelInstance* viewModelInstance() const { return m_viewModelInstance; }
    void addDependent(ViewModelValueDependent* value);
    void removeDependent(ViewModelValueDependent* value);
    virtual void setRoot(rcp<ViewModelInstance> value);
    void onValueChanged();

    // Where this value's latest change falls in the sequence that every value
    // change advances, or 0 if it never changed. State machines compare it with
    // how far their transition searches have got to tell whether a change, a
    // trigger fire included, is still pending for them (see
    // StateMachineInstance::viewModelValueChanged).
    uint64_t changeSequence() const { return m_changeSequence; }
    // The sequence's latest position: a change made after reading it gets a
    // greater one.
    static uint64_t latestChangeSequence()
    {
        return s_changeSequence.load(std::memory_order_relaxed);
    }
    // Takes the sequence's next position, for a change that isn't a view
    // model value's: a component trigger's fire (CustomPropertyTrigger).
    static uint64_t nextChangeSequence()
    {
        return s_changeSequence.fetch_add(1, std::memory_order_relaxed) + 1;
    }
    const std::string& name() const;
    const std::vector<ViewModelValueDependent*>& dependents() const
    {
        return m_DependencyHelper.dependents();
    }
};

class SuppressDelegation
{
public:
    SuppressDelegation(ViewModelInstanceValue* value) :
        m_value(value), m_suppressed(value->suppressDelegation())
    {}

    ~SuppressDelegation()
    {
        if (m_suppressed)
        {
            m_value->restoreDelegation();
        }
    }

private:
    ViewModelInstanceValue* m_value = nullptr;
    bool m_suppressed;
};

} // namespace rive

#endif