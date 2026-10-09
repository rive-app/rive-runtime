#ifndef _RIVE_WEIGHT_HPP_
#define _RIVE_WEIGHT_HPP_
#include "rive/generated/bones/weight_base.hpp"
#include "rive/math/vec2d.hpp"
#include <stdio.h>

namespace rive
{
class Vertex;
class Weight : public WeightBase
{
private:
    Vec2D m_Translation;

public:
    Vec2D& translation() { return m_Translation; }

    /// Whether [vertex] can deform through this weight: a cubic vertex reads
    /// its weight as a CubicWeight, so a base Weight under one can't be used.
    bool fitsVertex(const Vertex* vertex) const;

    StatusCode onAddedDirty(CoreContext* context) override;
#ifdef WITH_RIVE_EDITOR
    void editorParentChanged(ContainerComponent* from,
                             ContainerComponent* to) override;
    void valuesChanged() override { bindingChanged(); }
    void indicesChanged() override { bindingChanged(); }

protected:
    void bindingChanged();

public:
#endif

    static Vec2D deform(Vec2D inPoint,
                        unsigned int indices,
                        unsigned int weights,
                        const Mat2D& world,
                        const float* boneTransforms);
};
} // namespace rive

#endif