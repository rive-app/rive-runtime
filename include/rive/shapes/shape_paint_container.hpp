#ifndef _RIVE_SHAPE_PAINT_CONTAINER_HPP_
#define _RIVE_SHAPE_PAINT_CONTAINER_HPP_
#include "rive/refcnt.hpp"
#include "rive/shapes/path_flags.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/math/mat2d.hpp"
#include <vector>

namespace rive
{
class Artboard;
class ShapePaint;
class Component;

class CommandPath;

class ShapePaintContainer
{
    friend class ShapePaint;

protected:
    // Need this to access our artboard. We are treated as a mixin, either
    // as a Shape or Artboard, so both of those will override this.
    virtual Artboard* getArtboard() = 0;

    PathFlags m_pathFlags = PathFlags::none;
    std::vector<ShapePaint*> m_ShapePaints;
    void addPaint(ShapePaint* paint);

public:
    static ShapePaintContainer* from(Component* component);

    /// The component that's responsible for path building, helpful for adding
    /// dependencies after the paths are built.
    virtual Component* pathBuilder() = 0;

    virtual ~ShapePaintContainer() {}

    PathFlags pathFlags() const;

    void invalidateStrokeEffects();

    void propagateOpacity(float opacity);

    virtual const Mat2D& shapeWorldTransform() const = 0;

#ifdef TESTING
    const std::vector<ShapePaint*>& shapePaints() const
    {
        return m_ShapePaints;
    }
#endif

    virtual ShapePaintPath* worldPath() { return nullptr; }
    virtual ShapePaintPath* localPath() { return nullptr; }
    virtual ShapePaintPath* localClockwisePath() { return nullptr; }
};
} // namespace rive

#endif
