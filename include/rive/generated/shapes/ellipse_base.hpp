#ifndef _RIVE_ELLIPSE_BASE_HPP_
#define _RIVE_ELLIPSE_BASE_HPP_
#include "rive/shapes/parametric_path.hpp"
namespace rive
{
class EllipseBase : public ParametricPath
{
protected:
    typedef ParametricPath Super;

public:
    static const uint16_t typeKey = 4;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif