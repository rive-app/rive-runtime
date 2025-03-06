#ifndef _RIVE_N_SLICED_NODE_HPP_
#define _RIVE_N_SLICED_NODE_HPP_
#include "rive/generated/layout/n_sliced_node_base.hpp"
#include "rive/layout/n_slicer_details.hpp"
#include "rive/shapes/deformer.hpp"
#include <stdio.h>
namespace rive
{
class NSlicedNode : public NSlicedNodeBase,
                    public NSlicerDetails,
                    public RenderPathDeformer,
                    public PointDeformer
{
    void markPathDirtyRecursive(bool sendToLayout = true);
    void updateMapWorldPoint();

public:
    void axisChanged() override;
    void widthChanged() override;
    void heightChanged() override;
    void update(ComponentDirt value) override;

    std::function<void(Vec2D&)> mapWorldPoint = [](Vec2D& v) {};

    Component* asComponent() override { return this; };
    void deformWorldRenderPath(RawPath& path) const override;
    void deformLocalRenderPath(RawPath& path,
                               const Mat2D& worldTransform,
                               const Mat2D& inverseWorld) const override;
    Vec2D deformLocalPoint(Vec2D point,
                           const Mat2D& worldTransform,
                           const Mat2D& inverseWorld) const override;
    Vec2D deformWorldPoint(Vec2D point) const override;
    Vec2D scaleForNSlicer() const;

    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
    void controlSize(Vec2D size,
                     LayoutScaleType widthScaleType,
                     LayoutScaleType heightScaleType,
                     LayoutDirection direction) override;
    bool shouldPropagateSizeToChildren() override { return false; }
};
} // namespace rive

#endif