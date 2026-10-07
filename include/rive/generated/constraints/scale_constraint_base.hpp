#ifndef _RIVE_SCALE_CONSTRAINT_BASE_HPP_
#define _RIVE_SCALE_CONSTRAINT_BASE_HPP_
#include "rive/constraints/transform_component_constraint_y.hpp"
namespace rive
{
class ScaleConstraintBase : public TransformComponentConstraintY
{
protected:
    typedef TransformComponentConstraintY Super;

public:
    static const uint16_t typeKey = 88;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif