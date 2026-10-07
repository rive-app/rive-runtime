#ifndef _RIVE_TEXT_MODIFIER_BASE_HPP_
#define _RIVE_TEXT_MODIFIER_BASE_HPP_
#include "rive/component.hpp"
namespace rive
{
class TextModifierBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 160;

    uint16_t coreType() const override { return typeKey; }
};
} // namespace rive

#endif