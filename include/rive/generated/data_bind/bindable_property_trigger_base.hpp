#ifndef _RIVE_BINDABLE_PROPERTY_TRIGGER_BASE_HPP_
#define _RIVE_BINDABLE_PROPERTY_TRIGGER_BASE_HPP_
#include "rive/data_bind/bindable_property_integer.hpp"
namespace rive
{
class BindablePropertyTriggerBase : public BindablePropertyInteger
{
protected:
    typedef BindablePropertyInteger Super;

public:
    static const uint16_t typeKey = 503;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif