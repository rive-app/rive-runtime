#ifndef _RIVE_DATA_VALUE_LIST_HPP_
#define _RIVE_DATA_VALUE_LIST_HPP_
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"

#include <stdio.h>
namespace rive
{
class DataValueList : public DataValue
{
private:
    std::vector<rcp<ViewModelInstanceListItem>> m_value;

public:
    DataValueList() {};
    static const DataType typeKey = DataType::list;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::list;
    }
    std::vector<rcp<ViewModelInstanceListItem>>* value() { return &m_value; };
    void clear() { m_value.clear(); };
    void addItem(rcp<ViewModelInstanceListItem> item)
    {
        m_value.push_back(item);
    };
    // void value(std::vector<ViewModelInstanceListItem*>* value) { m_value =
    // value; };
    constexpr static std::vector<rcp<ViewModelInstanceListItem>>* defaultValue =
        nullptr;
};
} // namespace rive

#endif