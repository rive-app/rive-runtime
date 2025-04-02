#ifndef _RIVE_VIEW_MODEL_INSTANCE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_base.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/component.hpp"
#include "rive/refcnt.hpp"
#include <stdio.h>
namespace rive
{
class ViewModel;
class ViewModelInstance : public ViewModelInstanceBase,
                          public RefCnt<ViewModelInstance>
{
private:
    std::vector<ViewModelInstanceValue*> m_PropertyValues;
    ViewModel* m_ViewModel;

public:
    ~ViewModelInstance();
    void addValue(ViewModelInstanceValue* value);
    ViewModelInstanceValue* propertyValue(const uint32_t id);
    ViewModelInstanceValue* propertyValue(const std::string& name);
    bool replaceViewModelByName(const std::string& name,
                                rcp<ViewModelInstance> value);
    std::vector<ViewModelInstanceValue*> propertyValues();
    ViewModelInstanceValue* propertyFromPath(std::vector<uint32_t>* path,
                                             size_t index);
    void viewModel(ViewModel* value);
    ViewModel* viewModel() const;
    void onComponentDirty(Component* component);
    void setAsRoot(rcp<ViewModelInstance> instance);
    void setRoot(rcp<ViewModelInstance> value);
    Core* clone() const override;
    StatusCode import(ImportStack& importStack) override;
    void advanced();
};
} // namespace rive

#endif