#include "rive/generated/text/text_base.hpp"
#include "rive/text/text.hpp"

using namespace rive;

Core* TextBase::clone() const
{
    auto cloned = new Text();
    cloned->copy(*this);
    return cloned;
}
