#include "rive/generated/text/text_value_run_base.hpp"
#include "rive/text/text_value_run.hpp"

using namespace rive;

Core* TextValueRunBase::clone() const
{
    auto cloned = new TextValueRun();
    cloned->copy(*this);
    return cloned;
}
