#ifndef _RIVE_BLEND_MODE_HPP_
#define _RIVE_BLEND_MODE_HPP_
namespace rive
{
enum class BlendMode : unsigned char
{
    srcOver = 3,
    screen = 14,
    overlay = 15,
    darken = 16,
    lighten = 17,
    colorDodge = 18,
    colorBurn = 19,
    hardLight = 20,
    softLight = 21,
    difference = 22,
    exclusion = 23,
    multiply = 24,
    hue = 25,
    saturation = 26,
    color = 27,
    luminosity = 28
};
}
#endif
