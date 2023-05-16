#ifndef _RIVE_JOYSTICK_HPP_
#define _RIVE_JOYSTICK_HPP_
#include "rive/generated/joystick_base.hpp"
#include <stdio.h>
namespace rive
{
class Artboard;
class LinearAnimation;
class Joystick : public JoystickBase
{
public:
    void apply(Artboard* artboard) const;
    StatusCode onAddedClean(CoreContext* context) override;

private:
    LinearAnimation* m_xAnimation = nullptr;
    LinearAnimation* m_yAnimation = nullptr;
};
} // namespace rive

#endif