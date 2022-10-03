#include "rive/generated/draw_rules_base.hpp"
#include "rive/draw_rules.hpp"

using namespace rive;

Core* DrawRulesBase::clone() const
{
    auto cloned = new DrawRules();
    cloned->copy(*this);
    return cloned;
}
