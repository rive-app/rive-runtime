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
    click = 6,
    draggableConstraint = 7,
    textInput = 8,
    dragStart = 9,
    dragEnd = 10,
    viewModel = 11,
};
}
#endif