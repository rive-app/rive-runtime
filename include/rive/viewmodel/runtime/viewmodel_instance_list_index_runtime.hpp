#ifndef _RIVE_VIEW_MODEL_INSTANCE_LIST_INDEX_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_LIST_INDEX_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol_list_index.hpp"

namespace rive
{

class ViewModelInstanceListIndexRuntime : public ViewModelInstanceValueRuntime
{

public:
    ViewModelInstanceListIndexRuntime(
        ViewModelInstanceSymbolListIndex* viewModelInstance) :
        ViewModelInstanceValueRuntime(viewModelInstance)
    {}
    uint32_t value() const;
    const DataType dataType() override { return DataType::symbolListIndex; }
};
} // namespace rive
#endif
