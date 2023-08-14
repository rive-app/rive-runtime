#include "rive/generated/custom_property_string_base.hpp"
#include "rive/custom_property_string.hpp"

using namespace rive;

Core* CustomPropertyStringBase::clone() const
{
    auto cloned = new CustomPropertyString();
    cloned->copy(*this);
    return cloned;
}
