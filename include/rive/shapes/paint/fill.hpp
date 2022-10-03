#ifndef _RIVE_FILL_HPP_
#define _RIVE_FILL_HPP_
#include "rive/generated/shapes/paint/fill_base.hpp"
#include "rive/shapes/path_space.hpp"
namespace rive
{
class Fill : public FillBase
{
public:
    RenderPaint* initRenderPaint(ShapePaintMutator* mutator) override;
    PathSpace pathSpace() const override;
    void draw(Renderer* renderer, CommandPath* path) override;
};
} // namespace rive

#endif