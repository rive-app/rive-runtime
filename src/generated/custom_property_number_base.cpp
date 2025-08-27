#include "rive/generated/custom_property_number_base.hpp"
#include "rive/custom_property_number.hpp"

using namespace rive;

Core* CustomPropertyNumberBase::clone() const
{
    auto cloned = new CustomPropertyNumber();
    cloned->copy(*this);
    return cloned;
}
