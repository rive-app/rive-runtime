#ifndef _RIVE_DATA_BIND_MODE_HPP_
#define _RIVE_DATA_BIND_MODE_HPP_
namespace rive
{
enum class DataBindMode : unsigned int
{
    oneWay = 0,
    twoWay = 1,
    oneWayToSource = 2,
    once = 3,
};
}
#endif