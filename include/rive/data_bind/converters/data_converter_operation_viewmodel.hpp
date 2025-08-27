#ifndef _RIVE_DATA_CONVERTER_OPERATION_VIEW_MODEL_HPP_
#define _RIVE_DATA_CONVERTER_OPERATION_VIEW_MODEL_HPP_
#include "rive/generated/data_bind/converters/data_converter_operation_viewmodel_base.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterOperationViewModel
    : public DataConverterOperationViewModelBase
{
private:
    float resolveValue(DataBind* dataBind);
    ViewModelInstanceNumber* m_source = nullptr;

protected:
    std::vector<uint32_t> m_SourcePathIdsBuffer;

public:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataValue* reverseConvert(DataValue* value, DataBind* dataBind) override;
    void decodeSourcePathIds(Span<const uint8_t> value) override;
    void copySourcePathIds(
        const DataConverterOperationViewModelBase& object) override;
    const std::vector<uint32_t>& sourcePathIds()
    {
        return m_SourcePathIdsBuffer;
    }
    void bindFromContext(DataContext* dataContext, DataBind* dataBind) override;
};
} // namespace rive

#endif