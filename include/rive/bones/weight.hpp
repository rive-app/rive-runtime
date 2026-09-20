#ifndef _RIVE_WEIGHT_HPP_
#define _RIVE_WEIGHT_HPP_
#include "rive/generated/bones/weight_base.hpp"
#include "rive/math/vec2d.hpp"
#include <stdio.h>

namespace rive
{
class Weight : public WeightBase
{
private:
    Vec2D m_Translation;

public:
    Vec2D& translation() { return m_Translation; }

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