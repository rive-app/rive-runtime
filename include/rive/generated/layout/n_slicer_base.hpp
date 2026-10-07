#ifndef _RIVE_N_SLICER_BASE_HPP_
#define _RIVE_N_SLICER_BASE_HPP_
#include "rive/container_component.hpp"
namespace rive
{
class NSlicerBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 493;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif