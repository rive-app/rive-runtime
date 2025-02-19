#ifndef _RIVE_DATA_CONVERTER_STRING_TRIM_HPP_
#define _RIVE_DATA_CONVERTER_STRING_TRIM_HPP_
#include "rive/generated/data_bind/converters/data_converter_string_trim_base.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/trim_type.hpp"
#include <stdio.h>
#include <algorithm>
#include <cctype>
#include <locale>
namespace rive
{
class DataConverterStringTrim : public DataConverterStringTrimBase
{
public:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataType outputType() override { return DataType::string; };
    TrimType trimValue() { return (TrimType)trimType(); }
    void trimTypeChanged() override;

private:
    DataValueString m_output;
    inline void ltrim(std::string& s)
    {
        s.erase(s.begin(),
                std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }));
    }

    inline void rtrim(std::string& s)
    {
        s.erase(std::find_if(s.rbegin(),
                             s.rend(),
                             [](unsigned char ch) { return !std::isspace(ch); })
                    .base(),
                s.end());
    }
    inline void trim(std::string& s)
    {
        rtrim(s);
        ltrim(s);
    }
};
} // namespace rive

#endif