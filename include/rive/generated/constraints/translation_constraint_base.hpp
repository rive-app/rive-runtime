#ifndef _RIVE_TRANSLATION_CONSTRAINT_BASE_HPP_
#define _RIVE_TRANSLATION_CONSTRAINT_BASE_HPP_
#include "rive/constraints/transform_component_constraint_y.hpp"
namespace rive
{
class TranslationConstraintBase : public TransformComponentConstraintY
{
protected:
    typedef TransformComponentConstraintY Super;

public:
    static const uint16_t typeKey = 87;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif