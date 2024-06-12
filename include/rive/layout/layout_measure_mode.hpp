#ifndef _RIVE_LAYOUT_MEASURE_MODE_HPP_
#define _RIVE_LAYOUT_MEASURE_MODE_HPP_
namespace rive
{
enum class LayoutMeasureMode : uint8_t
{
    undefined = 0,
    exactly = 1,
    atMost = 2
};
}
#endif