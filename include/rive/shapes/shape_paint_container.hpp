#ifndef _RIVE_SHAPE_PAINT_CONTAINER_HPP_
#define _RIVE_SHAPE_PAINT_CONTAINER_HPP_
#include "rive/refcnt.hpp"
#include "rive/shapes/path_space.hpp"
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

    PathSpace m_DefaultPathSpace = PathSpace::Neither;
    std::vector<ShapePaint*> m_ShapePaints;
    void addPaint(ShapePaint* paint);

    // TODO: void draw(Renderer* renderer, PathComposer& composer);
public:
    static ShapePaintContainer* from(Component* component);

    virtual ~ShapePaintContainer() {}

    PathSpace pathSpace() const;

    void invalidateStrokeEffects();

    rcp<CommandPath> makeCommandPath(PathSpace space);

    void propagateOpacity(float opacity);

#ifdef TESTING
    const std::vector<ShapePaint*>& shapePaints() const { return m_ShapePaints; }
#endif
};
} // namespace rive

#endif
