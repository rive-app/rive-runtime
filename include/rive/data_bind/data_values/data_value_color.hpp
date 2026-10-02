#ifndef _RIVE_DATA_VALUE_COLOR_HPP_
#define _RIVE_DATA_VALUE_COLOR_HPP_
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/shapes/paint/color.hpp"

#include <stdio.h>
namespace rive
{
class DataValueColor : public DataValue
{
private:
    int m_value = 0;
    void setChannel(int shift, int value)
    {
        uint32_t mask = 0xFFu << shift;
        m_value = (int)(((uint32_t)m_value & ~mask) |
                        (((uint32_t)value & 0xFFu) << shift));
    }

public:
    DataValueColor(int value) : m_value(value) {};
    DataValueColor() {};
    static const DataType typeKey = DataType::color;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::color;
    }
    int value() { return m_value; };
    void value(int value) { m_value = value; };
    int alpha() { return (m_value >> 24) & 0xFF; }
    int red() { return (m_value >> 16) & 0xFF; }
    int green() { return (m_value >> 8) & 0xFF; }
    int blue() { return m_value & 0xFF; }
    // Channels keep their low 8 bits, like Color's, so they never spill.
    void alpha(int value) { setChannel(24, value); }
    void red(int value) { setChannel(16, value); }
    void green(int value) { setChannel(8, value); }
    void blue(int value) { setChannel(0, value); }
    bool compare(DataValue* comparand) override
    {
        if (comparand->is<DataValueColor>())
        {
            return comparand->as<DataValueColor>()->value() == m_value;
        }
        return false;
    }
    void interpolate(DataValue* to, DataValue* data, float f) override
    {
        if (to->is<DataValueColor>() && data->is<DataValueColor>())
        {
            auto fromValue = static_cast<ColorInt>(value());
            auto toValue =
                static_cast<ColorInt>(to->as<DataValueColor>()->value());
            auto mixedColor = colorLerp(fromValue, toValue, f);
            data->as<DataValueColor>()->value(mixedColor);
        }
    }
    void copyValue(DataValue* destination) override
    {
        if (destination->is<DataValueColor>())
        {
            destination->as<DataValueColor>()->value(value());
        }
    }
    static const int defaultValue = 0;
};
} // namespace rive

#endif