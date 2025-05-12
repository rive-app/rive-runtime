#include "rive/data_bind/converters/data_converter_number_to_list.hpp"
#include "rive/data_bind/data_values/data_value_list.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/file.hpp"
#include "rive/math/math_types.hpp"
#include "rive/refcnt.hpp"

using namespace rive;

DataValue* DataConverterNumberToList::convert(DataValue* input,
                                              DataBind* dataBind)
{
    auto output = new DataValueList();
    if (input->is<DataValueList>())
    {
        return input;
    }
    else if (input->is<DataValueNumber>())
    {
        if (m_file != nullptr)
        {
            auto inputNumber = input->as<DataValueNumber>();
            auto count = std::max(0, (int)std::floor(inputNumber->value()));
            auto viewModel = m_file->viewModel(viewModelId());
            if (viewModel != nullptr)
            {
                auto defaultInstance = viewModel->defaultInstance();
                for (int i = 0; i < count; i++)
                {
                    auto item = new ViewModelInstanceListItem();
                    auto copy = rcp<ViewModelInstance>(
                        defaultInstance->clone()->as<ViewModelInstance>());
                    item->viewModelInstance(copy);
                    output->value()->push_back(item);
                }
            }
        }
    }
    return output;
}

void DataConverterNumberToList::file(File* value) { m_file = value; }
File* DataConverterNumberToList::file() const { return m_file; }

Core* DataConverterNumberToList::clone() const
{
    auto clone = static_cast<DataConverterNumberToList*>(
        DataConverterNumberToListBase::clone());
    clone->file(file());
    return clone;
}