#ifndef _RIVE_RANDOM_MODE_HPP_
#define _RIVE_RANDOM_MODE_HPP_
namespace rive
{
enum class RandomMode : unsigned char
{
    once = 0,
    always = 1,
    sourceChange = 2,
};
}
#endif
