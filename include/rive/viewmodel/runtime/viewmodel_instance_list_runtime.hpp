#ifndef _RIVE_VIEW_MODEL_INSTANCE_LIST_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_LIST_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include <unordered_map>
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"

namespace rive
{
class ViewModelInstanceRuntime;

class ViewModelInstanceListRuntime : public ViewModelInstanceValueRuntime
{

public:
    ViewModelInstanceListRuntime(ViewModelInstanceList* viewModelInstance) :
        ViewModelInstanceValueRuntime(viewModelInstance)
    {}
    ViewModelInstanceRuntime* instanceAt(int index);
    void addInstance(ViewModelInstanceRuntime*);
    bool addInstanceAt(ViewModelInstanceRuntime*, int);
    void removeInstance(ViewModelInstanceRuntime*);
    void removeInstanceAt(int);
    void swap(uint32_t, uint32_t);
    size_t size() const;
    const DataType dataType() override { return DataType::list; }

private:
    std::unordered_map<rcp<ViewModelInstanceListItem>,
                       rcp<ViewModelInstanceRuntime>>
        m_itemsMap;
};
} // namespace rive
#endif
