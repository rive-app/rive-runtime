#include "rive/generated/constraints/translation_constraint_base.hpp"
#include "rive/constraints/translation_constraint.hpp"

using namespace rive;

Core* TranslationConstraintBase::clone() const
{
    auto cloned = new TranslationConstraint();
    cloned->copy(*this);
    return cloned;
}
