#ifndef _RIVE_DATA_CONTEXT_HPP_
#define _RIVE_DATA_CONTEXT_HPP_
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/data_resolver.hpp"
#include "rive/refcnt.hpp"

namespace rive
{
class DataBindPath;
class DataContext
{
private:
    DataContext* m_Parent = nullptr;
    rcp<ViewModelInstance> m_ViewModelInstance;

public:
    DataContext(rcp<ViewModelInstance> viewModelInstance);

    DataContext* parent() { return m_Parent; }
    void parent(DataContext* value) { m_Parent = value; }
    ViewModelInstanceValue* getViewModelProperty(
        const std::vector<uint32_t> path) const;
    ViewModelInstanceValue* getRelativeViewModelProperty(
        const std::vector<uint32_t> path,
        DataResolver* resolver) const;
    ViewModelInstanceValue* getViewModelProperty(DataBindPath* dataBindPath);
    rcp<ViewModelInstance> getViewModelInstance(
        const std::vector<uint32_t> path) const;
    rcp<ViewModelInstance> getViewModelInstance(DataBindPath*) const;
    rcp<ViewModelInstance> getRelativeViewModelInstance(
        const std::vector<uint32_t> path,
        DataResolver* resolver) const;
    void viewModelInstance(rcp<ViewModelInstance> value);
    void advanced();
    rcp<ViewModelInstance> viewModelInstance() { return m_ViewModelInstance; };

    ViewModelInstanceValue* viewModelValue()
    {
        if (m_Parent)
        {
            return m_Parent->viewModelValue();
        }
        return nullptr;
    }
};
} // namespace rive
#endif
