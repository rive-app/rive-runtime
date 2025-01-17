#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_string_trim.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"

using namespace rive;

DataValue* DataConverterStringTrim::convert(DataValue* input,
                                            DataBind* dataBind)
{
    if (input->is<DataValueString>())
    {
        auto inputValue = input->as<DataValueString>()->value();
        switch (trimValue())
        {
            case TrimType::start:
                ltrim(inputValue);
                break;
            case TrimType::end:
                rtrim(inputValue);
                break;
            case TrimType::all:
                trim(inputValue);
                break;
            default:
                break;
        }

        m_output.value(inputValue);
    }
    else
    {
        m_output.value(DataValueString::defaultValue);
    }
    return &m_output;
}