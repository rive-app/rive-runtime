#ifndef _RIVE_VIEW_MODEL_INSTANCE_VALUE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_VALUE_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_value_base.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
#include "rive/dependency_helper.hpp"
#include "rive/component.hpp"
#include "rive/component_dirt.hpp"
#include <stdio.h>
namespace rive
{
class DataBind;
class ViewModelInstance;
class ViewModelInstanceValue : public ViewModelInstanceValueBase
{
private:
    ViewModelProperty* m_ViewModelProperty;

protected:
    DependencyHelper<ViewModelInstance, DataBind> m_DependencyHelper;
    void addDirt(ComponentDirt value);

public:
    StatusCode import(ImportStack& importStack) override;
    void viewModelProperty(ViewModelProperty* value);
    ViewModelProperty* viewModelProperty();
    void addDependent(DataBind* value);
    virtual void setRoot(ViewModelInstance* value);
};
} // namespace rive

#endif