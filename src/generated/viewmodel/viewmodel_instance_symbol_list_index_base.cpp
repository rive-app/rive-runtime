#include "rive/generated/viewmodel/viewmodel_instance_symbol_list_index_base.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol_list_index.hpp"

using namespace rive;

Core* ViewModelInstanceSymbolListIndexBase::clone() const
{
    auto cloned = new ViewModelInstanceSymbolListIndex();
    cloned->copy(*this);
    return cloned;
}
