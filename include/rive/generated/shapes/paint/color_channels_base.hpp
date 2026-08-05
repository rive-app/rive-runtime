#ifndef _RIVE_COLOR_CHANNELS_BASE_HPP_
#define _RIVE_COLOR_CHANNELS_BASE_HPP_
#include <cstdint>
namespace rive
{
class Core;
class ColorChannelsBase
{
public:
    static ColorChannelsBase* from(Core* object);
    virtual int colorValue() const = 0;
    virtual void colorValue(int value) = 0;
    static const uint16_t colorRedPropertyKey = 118;
    static const uint32_t colorRedBitOffset = 16;
    static const uint32_t colorRedFieldMask = 16711680u;
    uint32_t colorRed() const
    {
        return (static_cast<uint32_t>(colorValue()) >> 16) & 255u;
    }
    void colorRed(uint32_t value)
    {
        if (value > 255u)
        {
            value = 255u;
        }
        const int _cur = colorValue();
        const int _fieldMask = static_cast<int>(16711680u);
        const int _next = static_cast<int>((_cur & ~_fieldMask) |
                                           ((value << 16) & _fieldMask));
        if (_cur != _next)
        {
            colorValue(_next);
        }
    }
    static const uint16_t colorGreenPropertyKey = 136;
    static const uint32_t colorGreenBitOffset = 8;
    static const uint32_t colorGreenFieldMask = 65280u;
    uint32_t colorGreen() const
    {
        return (static_cast<uint32_t>(colorValue()) >> 8) & 255u;
    }
    void colorGreen(uint32_t value)
    {
        if (value > 255u)
        {
            value = 255u;
        }
        const int _cur = colorValue();
        const int _fieldMask = static_cast<int>(65280u);
        const int _next = static_cast<int>((_cur & ~_fieldMask) |
                                           ((value << 8) & _fieldMask));
        if (_cur != _next)
        {
            colorValue(_next);
        }
    }
    static const uint16_t colorBluePropertyKey = 210;
    static const uint32_t colorBlueBitOffset = 0;
    static const uint32_t colorBlueFieldMask = 255u;
    uint32_t colorBlue() const
    {
        return (static_cast<uint32_t>(colorValue()) >> 0) & 255u;
    }
    void colorBlue(uint32_t value)
    {
        if (value > 255u)
        {
            value = 255u;
        }
        const int _cur = colorValue();
        const int _fieldMask = static_cast<int>(255u);
        const int _next = static_cast<int>((_cur & ~_fieldMask) |
                                           ((value << 0) & _fieldMask));
        if (_cur != _next)
        {
            colorValue(_next);
        }
    }
    static const uint16_t colorAlphaPropertyKey = 218;
    static const uint32_t colorAlphaBitOffset = 24;
    static const uint32_t colorAlphaFieldMask = 4278190080u;
    uint32_t colorAlpha() const
    {
        return (static_cast<uint32_t>(colorValue()) >> 24) & 255u;
    }
    void colorAlpha(uint32_t value)
    {
        if (value > 255u)
        {
            value = 255u;
        }
        const int _cur = colorValue();
        const int _fieldMask = static_cast<int>(4278190080u);
        const int _next = static_cast<int>((_cur & ~_fieldMask) |
                                           ((value << 24) & _fieldMask));
        if (_cur != _next)
        {
            colorValue(_next);
        }
    }
};
} // namespace rive

#endif