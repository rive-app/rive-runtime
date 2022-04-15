#ifndef _RIVE_EVENT_TYPE_HPP_
#define _RIVE_EVENT_TYPE_HPP_
namespace rive {
    enum class EventType : int {
        updateHover = -1,
        enter = 0,
        exit = 1,
        down = 2,
        up = 3,
    };
}
#endif