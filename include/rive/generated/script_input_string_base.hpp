#ifndef _RIVE_SCRIPT_INPUT_STRING_BASE_HPP_
#define _RIVE_SCRIPT_INPUT_STRING_BASE_HPP_
#include "rive/custom_property_string.hpp"
namespace rive
{
class ScriptInputStringBase : public CustomPropertyString
{
protected:
    typedef CustomPropertyString Super;

public:
    static const uint16_t typeKey = 627;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif