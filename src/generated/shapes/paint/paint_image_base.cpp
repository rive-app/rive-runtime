#include "rive/generated/shapes/paint/paint_image_base.hpp"
#include "rive/shapes/paint/paint_image.hpp"

using namespace rive;

Core* PaintImageBase::clone() const
{
    auto cloned = new PaintImage();
    cloned->copy(*this);
    return cloned;
}
