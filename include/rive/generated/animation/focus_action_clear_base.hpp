#ifndef _RIVE_FOCUS_ACTION_CLEAR_BASE_HPP_
#define _RIVE_FOCUS_ACTION_CLEAR_BASE_HPP_
#include "rive/animation/focus_action.hpp"
namespace rive
{
class FocusActionClearBase : public FocusAction
{
protected:
    typedef FocusAction Super;

public:
    static const uint16_t typeKey = 1037;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif