#ifndef _RIVE_DATA_CONVERTER_OPERATION_VIEW_MODEL_HPP_
#define _RIVE_DATA_CONVERTER_OPERATION_VIEW_MODEL_HPP_
#include "rive/generated/data_bind/converters/data_converter_operation_viewmodel_base.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_value_dependent.hpp"
#include "rive/sidecar.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterOperationViewModel
    : public DataConverterOperationViewModelBase,
      public ViewModelValueDependent
{
private:
  float resolveValue(DataBind* dataBind);
  void clearSource();
  rcp<ViewModelInstanceNumber> m_source = nullptr;

protected:
  // Shared with the converter this one was cloned from.
  SharedSidecar<std::vector<uint32_t>> m_sourcePathIds;

public:
  ~DataConverterOperationViewModel();
  DataValue* convert(DataValue* value, DataBind* dataBind) override;
  DataValue* reverseConvert(DataValue* value, DataBind* dataBind) override;
  void decodeSourcePathIds(Span<const uint8_t> value) override;
  void copySourcePathIds(
      const DataConverterOperationViewModelBase& object) override;
  const std::vector<uint32_t>& sourcePathIds() const
  {
    static const std::vector<uint32_t> empty;
    auto ids = m_sourcePathIds.get();
    return ids != nullptr ? *ids : empty;
  }
  void bindFromContext(DataContext* dataContext, DataBind* dataBind) override;
  void unbind() override;
  void addDirt(ComponentDirt value, bool recurse) override;
  void relinkDataBind() override {}
};
} // namespace rive

#endif
