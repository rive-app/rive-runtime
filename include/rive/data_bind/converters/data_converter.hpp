#ifndef _RIVE_DATA_CONVERTER_HPP_
#define _RIVE_DATA_CONVERTER_HPP_
#include "rive/generated/data_bind/converters/data_converter_base.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_context.hpp"
#include <stdio.h>
namespace rive
{
class DataBind;
class DataConverter : public DataConverterBase
{
public:
    ~DataConverter();
    virtual DataValue* convert(DataValue* value, DataBind* dataBind)
    {
        return value;
    };
    virtual DataValue* reverseConvert(DataValue* value, DataBind* dataBind)
    {
        return value;
    };
    virtual DataType outputType() { return DataType::none; };
    virtual void bindFromContext(DataContext* dataContext, DataBind* dataBind);
    StatusCode import(ImportStack& importStack) override;
    void addDataBind(DataBind* dataBind);
    std::vector<DataBind*> dataBinds() const { return m_dataBinds; }
    void markConverterDirty();
    virtual void update();
    void copy(const DataConverter& object);
    virtual bool advance(float elapsedTime);

private:
    std::vector<DataBind*> m_dataBinds;
    DataBind* m_parentDataBind;
};
} // namespace rive

#endif