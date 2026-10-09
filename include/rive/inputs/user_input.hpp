#ifndef _RIVE_USER_INPUT_HPP_
#define _RIVE_USER_INPUT_HPP_
#include "rive/generated/inputs/user_input_base.hpp"
#include <stdio.h>
namespace rive
{
class UserInput : public UserInputBase
{
public:
    // Not a Component, but the artboard's object list owns it.
    bool claimsArtboardSlot(ImportStack& importStack) const override
    {
        return true;
    }
};
} // namespace rive

#endif
