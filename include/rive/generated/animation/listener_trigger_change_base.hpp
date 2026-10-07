#ifndef _RIVE_LISTENER_TRIGGER_CHANGE_BASE_HPP_
#define _RIVE_LISTENER_TRIGGER_CHANGE_BASE_HPP_
#include "rive/animation/listener_input_change.hpp"
namespace rive
{
class ListenerTriggerChangeBase : public ListenerInputChange
{
protected:
    typedef ListenerInputChange Super;

public:
    static const uint16_t typeKey = 115;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif