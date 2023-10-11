#ifndef _RIVE_LISTENER_TYPE_HPP_
#define _RIVE_LISTENER_TYPE_HPP_
namespace rive
{
enum class ListenerType : int
{
    enter = 0,
    exit = 1,
    down = 2,
    up = 3,
    move = 4,
    event = 5,
};
}
#endif