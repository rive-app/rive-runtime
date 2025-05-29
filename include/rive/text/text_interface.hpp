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
};

} // namespace rive
#endif