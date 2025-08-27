#include "rive/generated/data_bind/converters/data_converter_operation_viewmodel_base.hpp"
#include "rive/data_bind/converters/data_converter_operation_viewmodel.hpp"

using namespace rive;

Core* DataConverterOperationViewModelBase::clone() const
{
    auto cloned = new DataConverterOperationViewModel();
    cloned->copy(*this);
    return cloned;
}
