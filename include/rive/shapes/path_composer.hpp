#ifndef _RIVE_PATH_COMPOSER_HPP_
#define _RIVE_PATH_COMPOSER_HPP_
#include "rive/component.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/refcnt.hpp"
#include "rive/math/raw_path.hpp"

namespace rive
{
class Shape;
class CommandPath;

class PathComposer : public Component
{

public:
    PathComposer(Shape* shape);
    Shape* shape() const { return m_shape; }
    void buildDependencies() override;
    void onDirty(ComponentDirt dirt) override;
    void update(ComponentDirt value) override;

    ShapePaintPath* localPath() { return &m_localPath; }
    ShapePaintPath* worldPath() { return &m_worldPath; }
    ShapePaintPath* localClockwisePath() { return &m_localClockwisePath; }

    void pathCollapseChanged();

private:
    Shape* m_shape;
    ShapePaintPath m_localPath;
    ShapePaintPath m_worldPath;
    ShapePaintPath m_localClockwisePath;
    bool m_deferredPathDirt;
};
} // namespace rive
#endif
