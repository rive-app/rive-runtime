#ifndef _RIVE_NODE_HPP_
#define _RIVE_NODE_HPP_
#include "rive/generated/node_base.hpp"

namespace rive
{
/// A Rive Node
class Node : public NodeBase
{
private:
    Mat2D m_LocalTransform = Mat2D();
    bool m_LocalTransformNeedsRecompute = false;

    void computeLocalTransform();

public:
    void setComputedLocalX(float value) override {}
    void setComputedLocalY(float value) override {}
    void setComputedWorldX(float value) override {}
    void setComputedWorldY(float value) override {}
    void setComputedWidth(float value) override {}
    void setComputedHeight(float value) override {}

    float computedLocalX() override { return localTransform()[4]; };
    float computedLocalY() override { return localTransform()[5]; };
    float computedWorldX() override { return worldTransform()[4]; };
    float computedWorldY() override { return worldTransform()[5]; };
    float computedWidth() override { return 0; };
    float computedHeight() override { return 0; };

    void updateWorldTransform() override
    {
        m_LocalTransformNeedsRecompute = true;
        Super::updateWorldTransform();
    }

    Mat2D localTransform();

protected:
    void xChanged() override;
    void yChanged() override;
    void computedLocalXChanged() override {}
    void computedLocalYChanged() override {}
    void computedWorldXChanged() override {}
    void computedWorldYChanged() override {}
    void computedWidthChanged() override {}
    void computedHeightChanged() override {}
};
} // namespace rive

#endif