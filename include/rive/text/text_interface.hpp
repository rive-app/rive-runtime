#ifndef _RIVE_TEXT_INTERFACE_HPP_
#define _RIVE_TEXT_INTERFACE_HPP_

#include "rive/math/aabb.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/math/mat2d.hpp"

namespace rive
{
class Core;

class TextInterface
{
public:
  static TextInterface* from(Core* component);
  virtual void markPaintDirty() = 0;
  virtual void markShapeDirty() = 0;
  virtual AABB localBounds() const = 0;
  // Called by a TextStyleGlobal before it destroys the style instance its
  // runs paint with, while the outgoing paint is still alive: drop every
  // cached pointer into it and re-shape against the incoming style.
  virtual void styleReferenceWillChange() {}
  // Called by a TextStyleGlobal once it shows a different style.
  virtual void styleReferenceDidChange() { markShapeDirty(); }
};

} // namespace rive
#endif