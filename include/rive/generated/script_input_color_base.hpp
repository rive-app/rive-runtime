#ifndef _RIVE_SCRIPT_INPUT_COLOR_BASE_HPP_
#define _RIVE_SCRIPT_INPUT_COLOR_BASE_HPP_
#include "rive/custom_property_color.hpp"
namespace rive
{
class ScriptInputColorBase : public CustomPropertyColor
{
protected:
    typedef CustomPropertyColor Super;

public:
    static const uint16_t typeKey = 626;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif