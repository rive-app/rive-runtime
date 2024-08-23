#ifndef _RIVE_VIEW_MODEL_INSTANCE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_base.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/component.hpp"
#include <stdio.h>
namespace rive
{
class ViewModel;
class ViewModelInstance : public ViewModelInstanceBase
{
private:
    std::vector<ViewModelInstanceValue*> m_PropertyValues;
    ViewModel* m_ViewModel;

public:
    void addValue(ViewModelInstanceValue* value);
    ViewModelInstanceValue* propertyValue(const uint32_t id);
    ViewModelInstanceValue* propertyValue(const std::string& name);
    std::vector<ViewModelInstanceValue*> propertyValues();
    ViewModelInstanceValue* propertyFromPath(std::vector<uint32_t>* path, size_t index);
    void viewModel(ViewModel* value);
    ViewModel* viewModel() const;
    void onComponentDirty(Component* component);
    void setAsRoot();
    void setRoot(ViewModelInstance* value);
    Core* clone() const override;
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif