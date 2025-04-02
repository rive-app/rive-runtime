#ifndef _RIVE_DATA_CONVERTER_TO_STRING_HPP_
#define _RIVE_DATA_CONVERTER_TO_STRING_HPP_
#include "rive/generated/data_bind/converters/data_converter_to_string_base.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/data_bind/data_values/data_value_color.hpp"
#include <stdio.h>
#include <sstream>
#include <cmath>
namespace rive
{

class _ColorConverter
{
public:
    _ColorConverter() {}
    void color(int value)
    {
        if (m_color != value)
        {
            m_color = value;
            h = -1;
            l = -1;
            s = -1;
        }
    }
    int alphaComponent() { return (m_color >> 24) & 0xFF; };
    int redComponent() { return (m_color >> 16) & 0xFF; };
    int greenComponent() { return (m_color >> 8) & 0xFF; };
    int blueComponent() { return m_color & 0xFF; };
    std::string alpha() { return std::to_string(alphaComponent()); }
    std::string red() { return std::to_string(redComponent()); }
    std::string green() { return std::to_string(greenComponent()); }
    std::string blue() { return std::to_string(blueComponent()); }
    std::string hue()
    {
        if (h == -1)
        {
            calculateHSL();
        }
        return std::to_string(h);
    }
    std::string luminance()
    {
        if (l == -1)
        {
            calculateHSL();
        }
        return std::to_string(l);
    }
    std::string saturation()
    {
        if (s == -1)
        {
            calculateHSL();
        }
        return std::to_string(s);
    }
    std::string toHex(int value)
    {
        std::stringstream ss;
        ss << std::uppercase << std::hex << value;
        std::string result = ss.str();
        if (result.length() < 2)
        {
            result.insert(0, 2 - result.length(), '0');
        }
        return result;
    }
    void alphaHex(std::stringstream& stream)
    {
        stream << toHex(alphaComponent());
    }
    void redHex(std::stringstream& stream) { stream << toHex(redComponent()); }
    void greenHex(std::stringstream& stream)
    {
        stream << toHex(greenComponent());
    }
    void blueHex(std::stringstream& stream)
    {
        stream << toHex(blueComponent());
    }

private:
    int h = 0;
    int l = 0;
    int s = 0;
    int m_color = 0;
    void calculateHSL()
    {
        float r = (float)redComponent() / 255.0f;
        float g = (float)greenComponent() / 255.0f;
        float b = (float)blueComponent() / 255.0f;
        float maxComponent = std::max(std::max(r, g), b);
        float minComponent = std::min(std::min(r, g), b);
        float delta = maxComponent - minComponent;
        float hue = 0;
        float lum = 0;
        float sat = 0;
        if (delta != 0.0f)
        {
            if (maxComponent == r)
            {
                hue = fmodf(((g - b) / delta), 6);
            }
            else if (maxComponent == g)
            {
                hue = (b - r) / delta + 2;
            }
            else
            {
                hue = (r - g) / delta + 4;
            }
        }

        h = std::round((hue * 60));
        if (h < 0)
        {
            h = h + 360;
        }

        lum = (maxComponent + minComponent) / 2;
        sat = delta == 0 ? 0 : delta / (1 - std::abs(2 * lum - 1));

        l = std::round(lum * 100);
        s = std::round(sat * 100);
    }
};

class DataConverterToString : public DataConverterToStringBase
{
public:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataType outputType() override { return DataType::string; };
    void decimalsChanged() override;
    void colorFormatChanged() override;

private:
    DataValue* convertNumber(DataValueNumber* value);
    DataValue* convertColor(DataValueColor* value);
    DataValueString m_output;
    _ColorConverter m_converter;
};
} // namespace rive

#endif