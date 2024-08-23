#ifndef _RIVE_CLIPPING_SHAPE_HPP_
#define _RIVE_CLIPPING_SHAPE_HPP_
#include "rive/renderer.hpp"
#include "rive/generated/shapes/clipping_shape_base.hpp"
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
    rcp<RenderPath> m_RenderPath;

    // The actual render path used for clipping, which may be different from
    // the stored render path. For example if there's only one clipping
    // shape, we don't build a whole new path for it.
    RenderPath* m_ClipRenderPath = nullptr;

public:
    Node* source() const { return m_Source; }
    const std::vector<Shape*>& shapes() const { return m_Shapes; }
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;
    void buildDependencies() override;
    void update(ComponentDirt value) override;

    RenderPath* renderPath() const { return m_ClipRenderPath; }
};
} // namespace rive

#endif
