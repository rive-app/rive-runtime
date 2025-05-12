#ifndef _RIVE_DATA_CONVERTER_NUMBER_TO_LIST_HPP_
#define _RIVE_DATA_CONVERTER_NUMBER_TO_LIST_HPP_
#include "rive/generated/data_bind/converters/data_converter_number_to_list_base.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include <stdio.h>
namespace rive
{
class File;

class DataConverterNumberToList : public DataConverterNumberToListBase
{
private:
    File* m_file = nullptr;

public:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataType outputType() override { return DataType::list; };
    void file(File*);
    File* file() const;
    Core* clone() const override;
};
} // namespace rive

#endif