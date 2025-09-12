#ifndef _RIVE_DATA_CONVERTER_GROUP_HPP_
#define _RIVE_DATA_CONVERTER_GROUP_HPP_
#include "rive/generated/data_bind/converters/data_converter_group_base.hpp"
#include "rive/data_bind/converters/data_converter_group_item.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_type.hpp"
#include <stdio.h>
namespace rive
{
class DataConverterGroup : public DataConverterGroupBase
{
public:
    ~DataConverterGroup();
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataValue* reverseConvert(DataValue* value, DataBind* dataBind) override;
    void addItem(DataConverterGroupItem* item);
    DataType outputType() override
    {
        if (m_items.size() > 0)
        {
            int currentIndex = int(m_items.size() - 1);
            while (currentIndex >= 0)
            {
                if (m_items[currentIndex]->converter()->outputType() !=
                    DataType::input)
                {
                    return m_items[currentIndex]->converter()->outputType();
                }
                currentIndex--;
            }
        };
        return Super::outputType();
    }
    const std::vector<DataConverterGroupItem*>& items() { return m_items; }
    Core* clone() const override;
    void bindFromContext(DataContext* dataContext, DataBind* dataBind) override;
    void initialize(DataType inputType) override;
    void unbind() override;
    void update() override;
    bool advance(float elapsedSeconds) override;

private:
    std::vector<DataConverterGroupItem*> m_items;
};
} // namespace rive

#endif