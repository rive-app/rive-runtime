#ifndef _RIVE_VIEW_MODEL_INSTANCE_VALUE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_VALUE_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_value_base.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
#include "rive/dependency_helper.hpp"
#include "rive/dirtyable.hpp"
#include "rive/component.hpp"
#include "rive/component_dirt.hpp"
#include "rive/refcnt.hpp"
#include <stdio.h>
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
    valueChanged = 1 << 1,
    delegatesChanged = 1 << 2,
    delegating = 1 << 3,
};
RIVE_MAKE_ENUM_BITSET(ValueFlags)

class SuppressDelegation;

class ViewModelInstanceValue : public ViewModelInstanceValueBase,
                               public RefCnt<ViewModelInstanceValue>,
                               public Triggerable
{
    friend class SuppressDelegation;

private:
    ViewModelProperty* m_ViewModelProperty;
    static std::string defaultName;
    ValueFlags m_changeFlags;
    std::vector<ViewModelInstanceValueDelegate*> m_delegates;
    std::vector<ViewModelInstanceValueDelegate*> m_delegatesCopy;

public:
    void addDelegate(ViewModelInstanceValueDelegate* delegate);
    void removeDelegate(ViewModelInstanceValueDelegate* delegate);

protected:
    DependencyHelper<rcp<ViewModelInstance>, Dirtyable> m_DependencyHelper;
    void addDirt(ComponentDirt value);

    // Suppress/restore calling delegates.
    bool suppressDelegation();
    void restoreDelegation();

public:
    StatusCode import(ImportStack& importStack) override;
    void viewModelProperty(ViewModelProperty* value);
    ViewModelProperty* viewModelProperty();
    void addDependent(Dirtyable* value);
    void removeDependent(Dirtyable* value);
    virtual void setRoot(rcp<ViewModelInstance> value);
    virtual void advanced();
    bool hasChanged();
    void onValueChanged();
    const std::string& name() const;
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
    ViewModelInstanceValue* m_value;
    bool m_suppressed;
};

} // namespace rive

#endif