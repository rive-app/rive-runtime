#ifndef _RIVE_TRANSLATION_CONSTRAINT_HPP_
#define _RIVE_TRANSLATION_CONSTRAINT_HPP_
#include "rive/generated/constraints/translation_constraint_base.hpp"
#include <stdio.h>
namespace rive
{
class TranslationConstraint : public TranslationConstraintBase
{
public:
    void constrain(TransformComponent* component) override;
};
} // namespace rive

#endif