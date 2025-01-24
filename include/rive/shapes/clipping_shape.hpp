#ifndef _RIVE_CLIPPING_SHAPE_HPP_
#define _RIVE_CLIPPING_SHAPE_HPP_
#include "rive/renderer.hpp"
#include "rive/generated/shapes/clipping_shape_base.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include <vector>

namespace rive
{
class Shape;
class Node;
class RenderPath;
class ClippingShape : public ClippingShapeBase
{
private:
    std::vector<Shape*> m_Shapes;
    Node* m_Source = nullptr;

public:
    Node* source() const { return m_Source; }
    const std::vector<Shape*>& shapes() const { return m_Shapes; }
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;
    void buildDependencies() override;
    void update(ComponentDirt value) override;

    ShapePaintPath* path() { return m_clipPath; }

private:
    ShapePaintPath m_path;
    ShapePaintPath* m_clipPath = nullptr;
};
} // namespace rive

#endif
