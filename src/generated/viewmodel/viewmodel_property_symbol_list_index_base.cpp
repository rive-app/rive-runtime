#include "rive/generated/viewmodel/viewmodel_property_symbol_list_index_base.hpp"
#include "rive/viewmodel/viewmodel_property_symbol_list_index.hpp"

using namespace rive;

Core* ViewModelPropertySymbolListIndexBase::clone() const
{
    auto cloned = new ViewModelPropertySymbolListIndex();
    cloned->copy(*this);
    return cloned;
}
