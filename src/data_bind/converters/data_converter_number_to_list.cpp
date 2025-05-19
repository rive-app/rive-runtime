#include "rive/data_bind/converters/data_converter_number_to_list.hpp"
#include "rive/data_bind/data_values/data_value_list.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/file.hpp"
#include "rive/math/math_types.hpp"
#include "rive/refcnt.hpp"

using namespace rive;

DataConverterNumberToList::~DataConverterNumberToList()
{
    for (auto& item : m_listItems)
    {
        item->unref();
    }
}

DataValue* DataConverterNumberToList::convert(DataValue* input,
                                              DataBind* dataBind)
{
    if (input->is<DataValueList>())
    {
        return input;
    }
    else if (input->is<DataValueNumber>())
    {
        m_output.clear();
        auto inputNumber = input->as<DataValueNumber>();
        auto count = std::max(0, (int)std::floor(inputNumber->value()));
        if (m_file != nullptr && m_file->viewModel(viewModelId()) != nullptr)
        {
            auto viewModel = m_file->viewModel(viewModelId());
            if (count > m_listItems.size())
            {
                auto defaultInstance = viewModel->defaultInstance();
                while (m_listItems.size() < count)
                {
                    auto item = new ViewModelInstanceListItem();
                    auto copy = rcp<ViewModelInstance>(
                        defaultInstance->clone()->as<ViewModelInstance>());
                    item->viewModelInstance(copy);
                    m_listItems.push_back(item);
                }
            }
            else if (count < m_listItems.size())
            {
                while (m_listItems.size() > count)
                {
                    auto item = m_listItems.back();
                    m_listItems.pop_back();
                    item->unref();
                }
            }
        }
        else
        {
            clearItems();
        }
        for (auto item : m_listItems)
        {
            m_output.addItem(item);
        }
        return &m_output;
    }
    return nullptr;
}

void DataConverterNumberToList::clearItems()
{
    for (auto& item : m_listItems)
    {
        item->unref();
    }
    m_listItems.clear();
}

void DataConverterNumberToList::viewModelIdChanged()
{
    // Clear the cached items if viewmodel changes
    clearItems();
    markConverterDirty();
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