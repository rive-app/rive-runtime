#ifndef _RIVE_SCRIPTED_NODE_BOUNDS_HPP_
#define _RIVE_SCRIPTED_NODE_BOUNDS_HPP_
#include "rive/math/aabb.hpp"
#include "rive/math/mat2d.hpp"

namespace rive
{
/// The box a script reads as a node's bounds: `local`, the node's own box in
/// its local space (its localBounds), mapped through `world` when the script
/// asked for worldBounds. A node with no size of its own reads a zero box,
/// which in world space sits at the node's origin. That includes a shape or
/// path with no geometry, whose localBounds is the inverted box that
/// AABB::forExpansion() starts from.
inline AABB scriptedNodeBounds(AABB local, const Mat2D* world)
{
  if (local.minX > local.maxX || local.minY > local.maxY)
  {
    local = AABB();
  }
  return world != nullptr ? world->mapBoundingBox(local) : local;
}
} // namespace rive

#endif
