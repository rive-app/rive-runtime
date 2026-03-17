#ifndef _RIVE_DATA_VALUE_VIEWMODEL_HPP_
#define _RIVE_DATA_VALUE_VIEWMODEL_HPP_
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"

#include <stdio.h>
namespace rive
{
class DataValueViewModel : public DataValue
{
private:
    ViewModelInstance* m_value = nullptr;

public:
    DataValueViewModel() {};
    static const DataType typeKey = DataType::viewModel;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::viewModel;
    }
    ViewModelInstance* value() { return m_value; };
    void value(ViewModelInstance* value) { m_value = value; };

    constexpr static ViewModelInstance* defaultValue = nullptr;
};
} // namespace rive

#endif