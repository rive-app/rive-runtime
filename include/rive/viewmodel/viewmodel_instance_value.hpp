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
namespace rive
{
class ViewModelInstance;
class ViewModelInstanceValue : public ViewModelInstanceValueBase,
                               public RefCnt<ViewModelInstanceValue>,
                               public Triggerable
{
private:
    ViewModelProperty* m_ViewModelProperty;
    static std::string defaultName;
    bool m_hasChanged = false;

protected:
    DependencyHelper<rcp<ViewModelInstance>, Dirtyable> m_DependencyHelper;
    void addDirt(ComponentDirt value);

public:
    StatusCode import(ImportStack& importStack) override;
    void viewModelProperty(ViewModelProperty* value);
    ViewModelProperty* viewModelProperty();
    void addDependent(Dirtyable* value);
    void removeDependent(Dirtyable* value);
    virtual void setRoot(rcp<ViewModelInstance> value);
    virtual void advanced();
    bool hasChanged() { return m_hasChanged; };
    void onValueChanged() { m_hasChanged = true; }
    const std::string& name() const;
};
} // namespace rive

#endif