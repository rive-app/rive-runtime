#ifndef _RIVE_VIEW_MODEL_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include "rive/file.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_runtime.hpp"
#include "rive/data_bind/data_values/data_type.hpp"

namespace rive
{
class ViewModel;

struct PropertyData
{
    DataType type;
    std::string name;
};

class ViewModelRuntime
{

public:
    ViewModelRuntime(ViewModel* viewModel, const File* file);

    const std::string& name() const;
    size_t instanceCount() const;
    size_t propertyCount() const;
    ViewModelInstanceRuntime* createInstanceFromIndex(size_t index) const;
    ViewModelInstanceRuntime* createInstanceFromName(
        const std::string& name) const;
    ViewModelInstanceRuntime* createDefaultInstance() const;
    ViewModelInstanceRuntime* createInstance() const;
    std::vector<PropertyData> properties();
    static std::vector<PropertyData> buildPropertiesData(
        std::vector<rive::ViewModelProperty*>& properties);
    std::vector<std::string> instanceNames() const;

private:
    ViewModel* m_viewModel;
    const File* m_file;
    mutable std::vector<rcp<ViewModelInstanceRuntime>>
        m_viewModelInstanceRuntimes;
    ViewModelInstanceRuntime* createRuntimeInstance(
        rcp<ViewModelInstance> instance) const;
};
} // namespace rive
#endif
