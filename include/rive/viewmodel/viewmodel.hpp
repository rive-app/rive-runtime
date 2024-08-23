#ifndef _RIVE_VIEW_MODEL_HPP_
#define _RIVE_VIEW_MODEL_HPP_
#include "rive/generated/viewmodel/viewmodel_base.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include <stdio.h>
namespace rive
{
class ViewModel : public ViewModelBase
{
private:
    std::vector<ViewModelProperty*> m_Properties;
    std::vector<ViewModelInstance*> m_Instances;

public:
    void addProperty(ViewModelProperty* property);
    ViewModelProperty* property(const std::string& name);
    ViewModelProperty* property(size_t index);
    void addInstance(ViewModelInstance* value);
    ViewModelInstance* instance(size_t index);
    ViewModelInstance* instance(const std::string& name);
    ViewModelInstance* defaultInstance();
};
} // namespace rive

#endif