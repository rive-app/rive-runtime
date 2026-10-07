#ifndef _RIVE_SCRIPT_INPUT_NUMBER_BASE_HPP_
#define _RIVE_SCRIPT_INPUT_NUMBER_BASE_HPP_
#include "rive/custom_property_number.hpp"
namespace rive
{
class ScriptInputNumberBase : public CustomPropertyNumber
{
protected:
    typedef CustomPropertyNumber Super;

public:
    static const uint16_t typeKey = 611;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif