#ifndef _RIVE_EASING_HPP_
#define _RIVE_EASING_HPP_
#include <cstdint>

namespace rive
{
enum class Easing : uint8_t
{
    easeIn = 0,
    easeOut = 1,
    easeInOut = 2
};
}
#endif