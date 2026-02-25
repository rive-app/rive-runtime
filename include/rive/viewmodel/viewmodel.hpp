#ifndef _RIVE_VIEW_MODEL_HPP_
#define _RIVE_VIEW_MODEL_HPP_
#include "rive/generated/viewmodel/viewmodel_base.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/symbol_type.hpp"
#include "rive/refcnt.hpp"
#include <stdio.h>
namespace rive
{
class File;
class ViewModel : public ViewModelBase, public RefCnt<ViewModel>
{
private:
    std::vector<ViewModelProperty*> m_Properties;
    std::vector<ViewModelInstance*> m_Instances;
    File* m_file = nullptr;

public:
    ~ViewModel();
    void addProperty(ViewModelProperty* property);
    ViewModelProperty* property(const std::string& name);
    ViewModelProperty* property(SymbolType symbolType);
    ViewModelProperty* property(size_t index);
    void addInstance(ViewModelInstance* value);
    ViewModelInstance* instance(size_t index);
    ViewModelInstance* instance(const std::string& name);
    rcp<ViewModelInstance> createInstance();
    rcp<ViewModelInstance> createFromInstance(const std::string& instanceName);
    void file(File* value) { m_file = value; };
#ifdef WITH_RIVE_TOOLS
    File* file() { return m_file; };
#endif
    ViewModelInstance* defaultInstance();
    size_t instanceCount() const;
    std::vector<ViewModelProperty*> properties() { return m_Properties; }
    std::vector<ViewModelInstance*> instances() { return m_Instances; }
};
} // namespace rive

#endif