#ifndef _RIVE_N_SLICED_NODE_HPP_
#define _RIVE_N_SLICED_NODE_HPP_
#include "rive/generated/layout/n_sliced_node_base.hpp"
#include "rive/layout/n_slicer_details.hpp"
#include "rive/shapes/shape.hpp"
#include <stdio.h>
namespace rive
{
class NSlicedNode : public NSlicedNodeBase,
                    public NSlicerDetails,
                    public ShapeDeformer
{
    void markPathDirtyRecursive(bool sendToLayout = true);

public:
    void axisChanged() override;
    void widthChanged() override;
    void heightChanged() override;

    void deformWorldRenderPath(RawPath& path) const override;
    void deformLocalRenderPath(RawPath& path,
                               const Mat2D& worldTransform,
                               const Mat2D& inverseWorld) const override;
    Vec2D scaleForNSlicer() const;
    Mat2D boundsTransform() const;

    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
    void controlSize(Vec2D size) override;
    bool shouldPropagateSizeToChildren() override { return false; }
};
} // namespace rive

#endif