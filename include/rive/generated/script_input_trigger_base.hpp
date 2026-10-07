#ifndef _RIVE_SCRIPT_INPUT_TRIGGER_BASE_HPP_
#define _RIVE_SCRIPT_INPUT_TRIGGER_BASE_HPP_
#include "rive/custom_property_trigger.hpp"
namespace rive
{
class ScriptInputTriggerBase : public CustomPropertyTrigger
{
protected:
    typedef CustomPropertyTrigger Super;

public:
    static const uint16_t typeKey = 618;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif