#ifndef _RIVE_CLIPPING_SHAPE_HPP_
#define _RIVE_CLIPPING_SHAPE_HPP_
#include "rive/generated/shapes/clipping_shape_base.hpp"
#include <stdio.h>
#include <vector>

namespace rive {
    class Shape;
    class Node;
    class RenderPath;
    class ClippingShape : public ClippingShapeBase {
    private:
        std::vector<Shape*> m_Shapes;
        Node* m_Source = nullptr;
        RenderPath* m_RenderPath = nullptr;

    public:
        ~ClippingShape();
        Node* source() const { return m_Source; }
        const std::vector<Shape*>& shapes() const { return m_Shapes; }
        StatusCode onAddedClean(CoreContext* context) override;
        StatusCode onAddedDirty(CoreContext* context) override;
        void buildDependencies() override;
        void update(ComponentDirt value) override;

        RenderPath* renderPath() const { return m_RenderPath; }
    };
} // namespace rive

#endif