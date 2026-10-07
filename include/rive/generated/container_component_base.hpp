#ifndef _RIVE_CONTAINER_COMPONENT_BASE_HPP_
#define _RIVE_CONTAINER_COMPONENT_BASE_HPP_
#include "rive/component.hpp"
namespace rive
{
class ContainerComponentBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 11;

    uint16_t coreType() const override { return typeKey; }
};
} // namespace rive

#endif