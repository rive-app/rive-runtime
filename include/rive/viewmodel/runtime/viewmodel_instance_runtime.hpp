#ifndef _RIVE_VIEW_MODEL_INSTANCE_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include <unordered_map>
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_boolean_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_color_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_number_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_string_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_enum_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_trigger_runtime.hpp"
#include "rive/refcnt.hpp"

namespace rive
{
class ViewModel;
struct PropertyData;

class ViewModelInstanceRuntime : public RefCnt<ViewModelInstanceRuntime>
{

public:
    ViewModelInstanceRuntime(rcp<ViewModelInstance> instance);
    ~ViewModelInstanceRuntime();

    const std::string& name() const;
    size_t propertyCount() const;
    ViewModelInstanceNumberRuntime* propertyNumber(
        const std::string& path) const;
    ViewModelInstanceStringRuntime* propertyString(
        const std::string& path) const;
    ViewModelInstanceBooleanRuntime* propertyBoolean(
        const std::string& path) const;
    ViewModelInstanceColorRuntime* propertyColor(const std::string& path) const;
    ViewModelInstanceEnumRuntime* propertyEnum(const std::string& path) const;
    ViewModelInstanceTriggerRuntime* propertyTrigger(
        const std::string& path) const;
    ViewModelInstanceRuntime* propertyViewModel(const std::string& path) const;
    bool replaceViewModel(const std::string& path,
                          ViewModelInstanceRuntime* value) const;
    bool replaceViewModelByName(const std::string& name,
                                ViewModelInstanceRuntime* value) const;
    ViewModelInstanceValueRuntime* property(const std::string& path) const;
    rcp<ViewModelInstance> instance() { return m_viewModelInstance; };
    std::vector<PropertyData> properties() const;

private:
    rcp<ViewModelInstance> m_viewModelInstance = nullptr;

    std::string getPropertyNameFromPath(const std::string& path) const;
    const ViewModelInstanceRuntime* viewModelInstanceFromFullPath(
        const std::string& path) const;

    mutable std::unordered_map<std::string, ViewModelInstanceValueRuntime*>
        m_properties;
    mutable std::unordered_map<std::string, rcp<ViewModelInstanceRuntime>>
        m_viewModelInstances;
    rcp<ViewModelInstance> viewModelInstanceProperty(
        const std::string& name) const;
    rcp<ViewModelInstanceRuntime> instanceRuntime(
        const std::string& name) const;
    ViewModelInstanceRuntime* viewModelInstanceAtPath(
        const std::string& path) const;
    template <typename T = ViewModelInstanceValue,
              typename U = ViewModelInstanceValueRuntime>
    U* getPropertyInstance(const std::string name) const
    {
        auto itr = m_properties.find(name);
        if (itr != m_properties.end())
        {
            return static_cast<U*>(itr->second);
        }
        auto viewModelInstanceValue = m_viewModelInstance->propertyValue(name);
        if (viewModelInstanceValue != nullptr &&
            viewModelInstanceValue->is<T>())
        {
            auto runtimeInstance = new U(viewModelInstanceValue->as<T>());
            m_properties[name] = runtimeInstance;
            return runtimeInstance;
        }
        return nullptr;
    };
};
} // namespace rive
#endif
