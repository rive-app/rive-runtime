#ifndef _RIVE_SKELETAL_COMPONENT_BASE_HPP_
#define _RIVE_SKELETAL_COMPONENT_BASE_HPP_
#include "rive/transform_component.hpp"
namespace rive
{
class SkeletalComponentBase : public TransformComponent
{
protected:
    typedef TransformComponent Super;

public:
    static const uint16_t typeKey = 39;

    uint16_t coreType() const override { return typeKey; }
};
} // namespace rive

#endif