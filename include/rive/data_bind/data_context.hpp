#ifndef _RIVE_DATA_CONTEXT_HPP_
#define _RIVE_DATA_CONTEXT_HPP_
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"

namespace rive
{
class DataContext
{
private:
    DataContext* m_Parent = nullptr;
    std::vector<ViewModelInstance*> m_ViewModelInstances;
    ViewModelInstance* m_ViewModelInstance;

public:
    DataContext();
    DataContext(ViewModelInstance* viewModelInstance);
    ~DataContext();

    DataContext* parent() { return m_Parent; }
    void parent(DataContext* value) { m_Parent = value; }
    void addViewModelInstance(ViewModelInstance* value);
    ViewModelInstanceValue* getViewModelProperty(const std::vector<uint32_t> path) const;
    ViewModelInstance* getViewModelInstance(const std::vector<uint32_t> path) const;
    void viewModelInstance(ViewModelInstance* value);
    ViewModelInstance* viewModelInstance() { return m_ViewModelInstance; };

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
