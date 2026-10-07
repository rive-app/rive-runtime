#ifndef _RIVE_CLAMPED_SCROLL_PHYSICS_BASE_HPP_
#define _RIVE_CLAMPED_SCROLL_PHYSICS_BASE_HPP_
#include "rive/constraints/scrolling/scroll_physics.hpp"
namespace rive
{
class ClampedScrollPhysicsBase : public ScrollPhysics
{
protected:
    typedef ScrollPhysics Super;

public:
    static const uint16_t typeKey = 524;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif