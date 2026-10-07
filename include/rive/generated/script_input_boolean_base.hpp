#ifndef _RIVE_SCRIPT_INPUT_BOOLEAN_BASE_HPP_
#define _RIVE_SCRIPT_INPUT_BOOLEAN_BASE_HPP_
#include "rive/custom_property_boolean.hpp"
namespace rive
{
class ScriptInputBooleanBase : public CustomPropertyBoolean
{
protected:
    typedef CustomPropertyBoolean Super;

public:
    static const uint16_t typeKey = 631;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif