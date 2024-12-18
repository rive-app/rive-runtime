#ifndef _RIVE_HITTABLE_HPP_
#define _RIVE_HITTABLE_HPP_

#include "rive/math/aabb.hpp"

namespace rive
{
class Component;

// A Component that can be hit-tested via two passes: a faster AABB pass, and a
// more accurate HiFi pass.
class Hittable
{
public:
    static Hittable* from(Component* component);
    virtual bool hitTestAABB(const Vec2D& position) = 0;
    virtual bool hitTestHiFi(const Vec2D& position, float hitRadius) = 0;
    virtual ~Hittable() {}
};
} // namespace rive

#endif
