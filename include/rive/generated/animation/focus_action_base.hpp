#ifndef _RIVE_FOCUS_ACTION_BASE_HPP_
#define _RIVE_FOCUS_ACTION_BASE_HPP_
#include "rive/animation/listener_action.hpp"
namespace rive
{
class FocusActionBase : public ListenerAction
{
protected:
    typedef ListenerAction Super;

public:
    static const uint16_t typeKey = 671;

    uint16_t coreType() const override { return typeKey; }
};
} // namespace rive

#endif