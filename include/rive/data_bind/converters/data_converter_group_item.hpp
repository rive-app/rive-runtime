#ifndef _RIVE_DATA_CONVERTER_GROUP_ITEM_HPP_
#define _RIVE_DATA_CONVERTER_GROUP_ITEM_HPP_
#include "rive/generated/data_bind/converters/data_converter_group_item_base.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterGroupItem : public DataConverterGroupItemBase
{
public:
    StatusCode import(ImportStack& importStack) override;
    DataConverter* converter() const { return m_dataConverter; };
    void converter(DataConverter* value) { m_dataConverter = value; };

protected:
    DataConverter* m_dataConverter;
};
} // namespace rive

#endif